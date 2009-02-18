/** Container File System Directory Management.
 *  @file dir.c
 *  
 *  @author Renaud Lottiaux 
 */

#include <linux/fs.h>
#include <linux/file.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/buffer_head.h>
#include "debug_kerfs.h"

#define MODULE_NAME "Ctnr FS dir     "

#ifdef KERFS_DIR_DEBUG
#define DEBUG_THIS_MODULE
#endif

#include <ctnr/container.h>
#include <ctnr/container_mgr.h>
#include "inode.h"
#include "dir.h"
#include "super.h"
#include "file_io_linker.h"



/** Add a new file in the given directory.
 *  @author Renaud Lottiaux
 *
 *  @param dir       Directory to add the file in.
 *  @param filename  Name of the file to add.
 *  @param ctnr      Container hosting the file.
 *  @param mode      File's mode.
 *  @param map_type  File mapping on disk type : local, replicated, etc.
 *
 *  @return  0 If everything ok. Negative value otherwise.
 */
int kerfs_add_dir_entry_no_lock (struct inode *dir,
                                 char *filename,
                                 container_t * ctnr,
                                 umode_t mode,
                                 int map_type)
{
  container_t *dir_ctnr = dir->i_mapping->a_ctnr;
  struct kerfs_dir_entry *dir_entry;
  struct page *page;
  void *page_addr;
  unsigned long pageid;
  int index;
  int offset;

  DEBUG (2, "Adding file %s to directory (ctnr %p) with mode %o\n", filename,
         ctnr, mode);

  ASSERT (dir_ctnr != NULL);

  index = dir->i_size;

  pageid = index / PAGE_SIZE;
  offset = index % PAGE_SIZE;

  /* Grab the page which will host the new directory entry */

  page = ctnr_grab_object (dir_ctnr->ctnrid, pageid);

  ASSERT (page != NULL);

  /* Fill the new directory entry */

  page_addr = kmap (page);
  dir_entry = (struct kerfs_dir_entry *) ((unsigned long) page_addr + offset);

  switch (mode & S_IFMT)
    {
      case S_IFSOCK:
        dir_entry->type = DT_SOCK;
        break;
      case S_IFLNK:
        dir_entry->type = DT_LNK;
        break;
      case S_IFREG:
        dir_entry->type = DT_REG;
        break;
      case S_IFBLK:
        dir_entry->type = DT_BLK;
        break;
      case S_IFDIR:
        dir_entry->type = DT_DIR;
        break;
      case S_IFCHR:
        dir_entry->type = DT_CHR;
        break;
      case S_IFIFO:
        dir_entry->type = DT_FIFO;
        break;
      default:
        dir_entry->type = DT_UNKNOWN;
    }

  if (ctnr)
    {
      dir_entry->ctnrid = ctnr->ctnrid;
      dir_entry->linked_node = ctnr->linked_node;
    }
  else
    dir_entry->ctnrid = CTNR_UNUSED;

  dir_entry->mode = mode;
  dir_entry->rec_len = KERFS_DIRENT_SIZE;
  dir_entry->name_len = strlen (filename);
  dir_entry->map_type = map_type;
  memcpy (dir_entry->name, filename, dir_entry->name_len);
  dir_entry->name[dir_entry->name_len] = 0;

  dir->i_size += KERFS_DIRENT_SIZE;
  update_physical_inode_attr (dir_ctnr, dir, ATTR_SIZE);

  ctnr_sync_object (dir_ctnr->ctnrid, pageid);

  ctnr_put_object (dir_ctnr->ctnrid, pageid);
  kunmap (page);

  DEBUG (2, "Adding file %s to directory : done\n", filename);

  return 0;
}



int kerfs_add_dir_entry (struct inode *dir,
                         char *filename,
                         container_t * ctnr,
                         umode_t mode,
                         int map_type)
{
  int res;

  /* Grab the directory inode struct */

  grab_krg_inode_struct (dir);

  res = kerfs_add_dir_entry_no_lock (dir, filename, ctnr, mode, map_type);

  /* Put the directory inode struct */

  put_krg_inode_struct (dir);

  return res;
}



/** Remove a file entry from the given directory.
 *  @author Renaud Lottiaux
 *
 *  @param dir       Directory to remove the file from.
 *  @param inode     Inode hosting the file to remove.
 *
 *  @return  0 If everything ok. Negative value otherwise.
 *  The function MUST be called with dir inode already got through
 *  get_krg_inode_struct.
 */
int kerfs_delete_dir_entry_no_lock (struct inode *dir, struct inode *inode)
{
  container_t *dir_ctnr = dir->i_mapping->a_ctnr;
  struct kerfs_dir_entry *dir_entry, *last_dir_entry;
  int nr_entry_per_page;
  int n, i, nr_entry;
  void *page_addr, *last_page_addr;
  struct page *page, *last_page = NULL;
  int pageid = 0, last_pageid;
  int index;
  int offset;
  int r = 0;

  DEBUG (3, "Remove file entry %ld in directory %ld\n", inode->i_ino,
         dir->i_ino);

  nr_entry_per_page = PAGE_SIZE / KERFS_DIRENT_SIZE;

  nr_entry = dir->i_size / KERFS_DIRENT_SIZE;

  /* Lookup for the dir entry to remove. */

  for (i = 0; i < nr_entry;)
    {
      page = ctnr_grab_object (dir_ctnr->ctnrid, pageid);
      page_addr = kmap_atomic (page, KM_USER0);
      dir_entry = (struct kerfs_dir_entry *) page_addr;

      for (n = 0; n < nr_entry_per_page && i < nr_entry; n++, i++)
        {
          if (dir_entry->ctnrid == inode->i_ino)
            goto found;

          /* Switch to next directory entry */
          dir_entry = next_dir_entry (dir_entry);
        }

      /* Switch to next directory entry page */

      kunmap_atomic (page_addr, KM_USER0);
      ctnr_put_object (dir_ctnr->ctnrid, pageid);
      pageid++;
    }

  put_krg_inode_struct (dir);

  r = -ENOENT;
  goto exit;

found:

  /* We found the dir entry, let's remove it ! */

  index = dir->i_size - KERFS_DIRENT_SIZE;

  last_pageid = index / PAGE_SIZE;
  offset = index % PAGE_SIZE;

  /* Grab the page hosting the last directory entry */

  if (last_pageid != pageid)
    {
      last_page = ctnr_grab_object (dir_ctnr->ctnrid, last_pageid);
      last_page_addr = kmap_atomic (last_page, KM_USER1);
    }
  else
    last_page_addr = page_addr;

  /* Copy the last entry in the one to remove */

  last_dir_entry = (struct kerfs_dir_entry *) ((unsigned long) last_page_addr
                                               + offset);

  if (last_dir_entry != dir_entry)
    memcpy (dir_entry, last_dir_entry, KERFS_DIRENT_SIZE);

  dir->i_size = dir->i_size - KERFS_DIRENT_SIZE;
  update_physical_inode_attr (dir_ctnr, dir, ATTR_SIZE);

  /* Sync directory modified pages */
  ctnr_sync_object (dir_ctnr->ctnrid, pageid);
  if (last_pageid != pageid)
    {
      ctnr_sync_object (dir_ctnr->ctnrid, last_pageid);
      kunmap_atomic (last_page_addr, KM_USER1);
      ctnr_put_object (dir_ctnr->ctnrid, last_pageid);
    }

  kunmap_atomic (page_addr, KM_USER0);
  ctnr_put_object (dir_ctnr->ctnrid, pageid);

exit:
  DEBUG (3, "Remove file entry %ld in directory %ld : done with err %d\n",
         inode->i_ino, dir->i_ino, r);

  return r;
}



int kerfs_delete_dir_entry (struct inode *dir,
                            struct inode *inode)
{
  int res;

  /* Grab the directory inode struct */

  grab_krg_inode_struct (dir);

  res = kerfs_delete_dir_entry_no_lock (dir, inode);

  /* Put the directory inode struct */

  put_krg_inode_struct (dir);

  return res;
}



/** Lookup for a filename in the given directory and return the corresponding
 *  grabed dir entry.
 *  @author Renaud Lottiaux
 *
 *  @param dir       Directory to search the file in.
 *  @param filename  Name of the file to lookup for.
 *
 *  @return The kerfs directory entry of the file if found.
 *          Null otherwise.
 *
 *  The function MUST be called with dir inode already got through
 *  get_krg_inode_struct.
 */
struct kerfs_dir_entry *kerfs_find_grab_dir_entry_by_name (struct inode *dir,
                                                           char *filename)
{
  container_t *dir_ctnr = dir->i_mapping->a_ctnr;
  struct kerfs_dir_entry *dir_entry;
  int nr_entry_per_page;
  int n = 0, i, nr_entry;
  int filename_len;
  void *page_addr;
  int pageid = 0;
  ctnrObj_t *objEntry;
  int dir_entry_offset;

  DEBUG (3, "Look up for file %s in directory %ld\n", filename, dir->i_ino);

  nr_entry_per_page = PAGE_SIZE / KERFS_DIRENT_SIZE;

  nr_entry = dir->i_size / KERFS_DIRENT_SIZE;

  filename_len = strlen (filename);

  for (i = 0; i < nr_entry;)
    {
      struct page *page;

      page = ctnr_get_object (dir_ctnr->ctnrid, pageid);
      page_addr = kmap (page);
      dir_entry = (struct kerfs_dir_entry *) page_addr;

      for (n = 0; n < nr_entry_per_page && i < nr_entry; n++, i++)
        {
          if (dir_entry->name_len == filename_len &&
              strncmp (dir_entry->name, filename, filename_len) == 0)
            {
              /* Compute offset in the page */
              dir_entry_offset = (void *) dir_entry - page_addr;

              /* Grab the page */
              ctnr_put_object (dir_ctnr->ctnrid, pageid);
              kunmap (page);
              page = ctnr_grab_object (dir_ctnr->ctnrid, pageid);

              /* Get the dir entry in the grabbed page */
              page_addr = kmap (page);
              dir_entry = page_addr + dir_entry_offset;

              objEntry = __get_ctnr_object_entry (dir_ctnr, pageid);

              dir_entry->objEntry = objEntry;
              dir_entry->ctnr = dir_ctnr;
              goto found;
            }

          /* Switch to next directory entry */
          dir_entry = next_dir_entry (dir_entry);
        }

      /* Switch to next directory entry page */

      ctnr_put_object (dir_ctnr->ctnrid, pageid);
      kunmap (page);
      pageid++;
    }

  dir_entry = NULL;

found:
  DEBUG (3, "Look up for file %s in directory %ld : found : %p\n",
         filename, dir->i_ino, dir_entry);

  return dir_entry;
}


void kerfs_put_dir_entry (struct kerfs_dir_entry *dir_entry)
{
  __ctnr_put_object (dir_entry->ctnr, dir_entry->objEntry, 0);
  kunmap (dir_entry->objEntry->object);
}



/** Check if a directory is empty
 *  @author Renaud Lottiaux
 *
 *  @param dir      The directory to check.
 *
 *  @return 1 if the directory is empty, 0 otherwise.
 */
int kerfs_empty_dir (struct inode *dir)
{
  if (dir->i_size == 0)
    return 1;

  return 0;
}



/* Dirty compatibility */
extern int local_dir (struct dentry *dentry);


int ctnr_fill_dir (void *__buf,
                   const char *name,
                   int namlen,
                   loff_t offset,
                   ino_t ino,
                   unsigned int d_type)
{
  struct inode *dir;
  char filename[KERFS_NAME_LEN];

  dir = __buf;

  memcpy (filename, name, namlen);
  filename[namlen] = '\0';

  if (S_ISDIR (d_type))
    kerfs_add_dir_entry (dir, filename, NULL, d_type, KERFS_REPLICATED_FILE);
  else
    kerfs_add_dir_entry (dir, filename, NULL, d_type, KERFS_DISTRIBUTED_FILE);

  return 0;
}


int kerfs_fill_dir (struct inode *dir,
                    struct file *phys_dir)
{
  return 0;
  phys_dir->f_pos = 0;
  phys_dir->f_op->readdir (phys_dir, dir, ctnr_fill_dir);

  return 0;
}



/** Function called by the VFS when a readdir is performed.
 *  @author Renaud Lottiaux
 *
 *  @param filp      File hosting the directory.
 *  @param dirent    Buffer to write directory data to.
 *  @param filldir   Function used to write directory data in the given buffer.
 */
static int kerfs_readdir (struct file *filp,
                          void *dirent,
                          filldir_t filldir)
{
  struct dentry *dentry = filp->f_dentry;
  struct inode *dir = dentry->d_inode;
  container_t *dir_ctnr = dir->i_mapping->a_ctnr;
  unsigned long pageid;
  int nr_entry_per_page;
  void *page_addr;
  ino_t ino;
  int n, n0, i, nr_entry;

  if (dir_ctnr == NULL)
    goto done;

  DEBUG (2, "KERFS : readdir %s : offset = %d - dir size %d\n",
         filp->f_dentry->d_name.name, (int) filp->f_pos, (int) dir->i_size);

  i = filp->f_pos;

  switch (i)
    {
      case 0:
        ino = dir->i_ino;
        if (filldir (dirent, ".", 1, 0, ino, DT_DIR) < 0)
          break;
        filp->f_pos++;

        /* fallthrough */

      case 1:
        spin_lock (&dcache_lock);
        ino = dentry->d_parent->d_inode->i_ino;
        spin_unlock (&dcache_lock);
        if (filldir (dirent, "..", 2, 1, ino, DT_DIR) < 0)
          break;
        filp->f_pos++;

        /* fallthrough */

      default:
        i = filp->f_pos - 2;
        nr_entry_per_page = PAGE_SIZE / KERFS_DIRENT_SIZE;
        pageid = i / nr_entry_per_page;
        n0 = i % nr_entry_per_page;

        /* Get the directory inode struct */

        get_krg_inode_struct (dir);

        nr_entry = dir->i_size / KERFS_DIRENT_SIZE;

        while (i < nr_entry)
          {
            struct kerfs_dir_entry *dir_entry;
            struct page *page;

            page = ctnr_get_object (dir_ctnr->ctnrid, pageid);
            page_addr = kmap_atomic (page, KM_USER0);
            dir_entry = (struct kerfs_dir_entry *) page_addr;

            for (n = 0; n < n0; n++)
              dir_entry = next_dir_entry (dir_entry);

            for (n = n0; n < nr_entry_per_page && i < nr_entry; n++)
              {
                if (dir_entry->ctnrid == CTNR_UNUSED)
                  ino = 1;
                else
                  ino = dir_entry->ctnrid;

                /* Fill the buffer with the current directory entry */
                if (filldir (dirent, dir_entry->name, dir_entry->name_len,
                             filp->f_pos, ino, dir_entry->type) < 0)
                  {
                    ctnr_put_object (dir_ctnr->ctnrid, pageid);
                    put_krg_inode_struct (dir);
                    kunmap_atomic (page_addr, KM_USER0);
                    goto done;
                  }

                /* Switch to next directory entry */
                i++;
                filp->f_pos++;
                dir_entry = next_dir_entry (dir_entry);
              }
            n0 = 0;

            ctnr_put_object (dir_ctnr->ctnrid, pageid);
            kunmap_atomic (page_addr, KM_USER0);
            pageid++;
          }
        put_krg_inode_struct (dir);
    }
done:
  DEBUG (2, "KERFS : readdir %s : offset = %d - dir size %d : done\n",
         filp->f_dentry->d_name.name, (int) filp->f_pos, (int) dir->i_size);

  return 0;
}



struct file_operations kerfs_dir_fops = {
  llseek:  dcache_dir_lseek,
  read:    generic_read_dir,
  open:    dcache_dir_open,
  release: dcache_dir_close,
  readdir: kerfs_readdir,
  fsync:   file_fsync,
};
