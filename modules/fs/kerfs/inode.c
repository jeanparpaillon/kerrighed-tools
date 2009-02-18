/** Container File System inode management.
 *  @file inode.c
 *  
 *  @author Renaud Lottiaux 
 */

#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/quotaops.h>
#include <linux/nfs_fs.h>

#include "debug_kerfs.h"

#define MODULE_NAME "Ctnr FS inode   "

#ifdef KERFS_INODE_DEBUG
#define DEBUG_THIS_MODULE
#endif

#include <ctnr/container_mgr.h>
#include "dir_io_linker.h"
#include "file_io_linker.h"
#include "file_int_linker.h"
#include "address_space.h"
#include "dir.h"
#include "inode.h"
#include "dentry.h"
#include "file.h"
#include "super.h"
#include "physical_fs.h"



/* Container of inode structures */
container_t *inode_struct_ctnr = NULL;



/*****************************************************************************/
/*                                                                           */
/*                      GLOBAL INODE MANAGEMENT FUNCTIONS                    */
/*                                                                           */
/*****************************************************************************/



void kerfs_init_krg_inode_struct (ctnrid_t ctnrid,
                                  struct inode *inode)
{
  struct inode *ctnr_inode;

  DEBUG (2, "KERFS : init inode struct %p (objid %ld)\n", inode, ctnrid);

  ctnr_inode = ctnr_grab_object (INODE_STRUCT_CTNR_ID, ctnrid);

  if (inode)
    {
      inode->i_objid = ctnrid;
      kerfs_copy_inode (ctnr_inode, inode);
      DEBUG (2, "KERFS : inode %ld exist and has size %ld\n", ctnrid,
             (long int) inode->i_size);
    }
  else
    {
      ctnr_inode->i_size = 0;
      ctnr_inode->i_atime = CURRENT_TIME;
      ctnr_inode->i_mtime = CURRENT_TIME;
      ctnr_inode->i_ctime = CURRENT_TIME;
      ctnr_inode->i_version = 0;
      DEBUG (2, "KERFS : inode %ld has size 0\n", ctnrid);
    }

  ctnr_put_object (INODE_STRUCT_CTNR_ID, ctnrid);

  DEBUG (2, "KERFS : init inode struct (objid %ld) done\n", ctnrid);
}



/*****************************************************************************/
/*                                                                           */
/*                           INODE TOOLS FUNCTIONS                           */
/*                                                                           */
/*****************************************************************************/



/** Allocate a kerfs inode.
 *  @author Renaud Lottiaux
 *
 *  @param sb       Super Block of the file system to allocate a inode for.
 *  @param mode     Mode of the created inode.
 *  @param ctn      Container hosting the file.
 *
 *  @return The newly allocated inode.
 */
struct inode *kerfs_get_inode (struct super_block *sb,
                               int mode,
                               container_t * ctnr)
{
  //  struct inode *inode = new_inode (sb);
  struct inode *inode;
  ctnrid_t ctnrid;

  ASSERT(sb != NULL);

  inode = new_inode (sb);

  DEBUG (2, "KERFS : get_inode\n");

  if (inode)
    {
      if (ctnr == NULL)
        ctnrid = alloc_new_ctnr_id ();
      else
        ctnrid = ctnr->ctnrid;

      inode->i_ino = ctnrid;
      inode->i_size = 0;
      inode->i_mode = mode;
      inode->i_uid = current->fsuid;
      inode->i_gid = current->fsgid;
      inode->i_blksize = PAGE_CACHE_SIZE;
      inode->i_blocks = 0;
      inode->i_mapping->a_ops = &kerfs_aops ;
      inode->i_atime = inode->i_mtime = inode->i_ctime = CURRENT_TIME;

      if (ctnr != NULL)
        {
          inode->i_objid = ctnrid;

          inode->i_mapping->a_ctnr = ctnr;
        }

      switch (mode & S_IFMT)
        {
          default:
            PANIC ("Cannot set kerfs inode mode to %o\n", mode & S_IFMT);
            break;

          case S_IFREG:
            inode->i_op = &kerfs_file_inode_operations;
            inode->i_fop = &kerfs_file_fops;
            break;

          case S_IFDIR:
            inode->i_op = &kerfs_dir_inode_operations;
            inode->i_fop = &kerfs_dir_fops;
            break;

          case S_IFLNK:
            inode->i_op = &kerfs_link_inode_operations;
            break;
        }
    }

  insert_inode_hash (inode);

  DEBUG (2, "KERFS : get_inode : done ino = %ld\n", inode->i_ino);

  return inode;
}



static inline void kerfs_inc_count (struct inode *inode)
{
  inode->i_nlink++;
  mark_inode_dirty (inode);
}



static inline void kerfs_dec_count (struct inode *inode)
{
  inode->i_nlink--;
  mark_inode_dirty (inode);
}



/*****************************************************************************/
/*                                                                           */
/*                         INODE OPERATION FUNCTIONS                         */
/*                                                                           */
/*****************************************************************************/



/** File system dependent mknod function called by the Linux VFS.
 *  @author Renaud Lottiaux
 *
 *  @param dir      Directory to create the inode in.
 *  @param dentry   Dentry of the file to create.
 *  @param mode     Mode of the file to create.
 *  @param dev      Device associated to the file.
 */
int kerfs_mknod (struct inode *dir,
                 struct dentry *dentry,
                 int mode,
                 int dev)
{
  struct inode *inode;
  container_t *ctnr;
  int error = -ENOSPC;

  DEBUG (2, "KERFS : mknod\n");

  ctnr = find_container (dev);
  ASSERT (ctnr != NULL);
  inode = get_file_ctnr_inode (ctnr);

  if (inode)
    {
      get_krg_inode_struct (inode);
      put_krg_inode_struct (inode);

      if (dir->i_mode & S_ISGID)
        {
          inode->i_gid = dir->i_gid;
          if (S_ISDIR (mode))
            inode->i_mode |= S_ISGID;
        }

      /* One extra inc to take into account the use of the inode
       * by the container */
      atomic_inc (&inode->i_count);
      d_instantiate (dentry, inode);
      error = 0;
    }
  else
    PANIC ("No inode found\n");

  DEBUG (2, "KERFS : mknod : done %d links (dentry count %d) on inode\n",
         inode->i_nlink, atomic_read (&dentry->d_count));

  return error;
}



/** File system dependent create function called by the Linux VFS.
 *  @author Renaud Lottiaux
 *
 *  @param dir      Directory to create the inode in.
 *  @param dentry   Dentry of the file to create.
 *  @param mode     Mode of the file to create.
 */
static int kerfs_create (struct inode *dir,
                         struct dentry *dentry, 
                         int mode,
                         struct nameidata *nd)
{
  struct vfsmount *mnt;
  container_t *ctnr;
  int err = 0;

  DEBUG (2, "KERFS : create %s\n", dentry->d_name.name);

  mnt = get_dir_ctnr_mnt (dir);

  mode |= S_IFREG;

  ctnr = create_file_container (dentry, mnt, 1, kerrighed_node_id, O_CREAT,
                                mode, current->fsuid, current->fsgid);

  if (IS_ERR (ctnr))
    return PTR_ERR (ctnr);

  err = kerfs_mknod (dir, dentry, mode, ctnr->ctnrid);
  if (err == 0)
    kerfs_add_dir_entry (dir, (char *) dentry->d_name.name, ctnr,
                         mode, KERFS_DISTRIBUTED_FILE);

  DEBUG (2,
         "KERFS : create %s : done (ctnr %ld mode %o - uid %d - gid %d) with err %d\n",
         dentry->d_name.name, ctnr->ctnrid, dentry->d_inode->i_mode,
         dentry->d_inode->i_uid, dentry->d_inode->i_gid, err);

  return err;
}



/** Kerfs lookup function.
 *  @author Renaud Lottiaux
 *
 *  @param dir       Directory to lookup in.
 *  @param dentry    Dentry of the file to lookup for.
 *  @param dir_entry The kerfs directory entry hosting the file.
 */
struct inode *do_kerfs_lookup (struct inode *dir,
                               struct dentry *dentry,
                               struct kerfs_dir_entry *dir_entry)
{
  struct inode *inode = NULL;
  struct vfsmount *mnt;
  container_t *ctnr;
  struct file *file;

  DEBUG (3, "Look up for file %s\n", dentry->d_name.name);

  mnt = get_dir_ctnr_mnt (dir);

  switch (dir_entry->map_type)
    {

      case KERFS_LOCAL_FILE:
        file = open_physical_file2 (dentry, mnt, O_RDONLY, 0644,
                                    current->fsuid, current->fsgid);
        if (IS_ERR (file))
          goto done;

        inode = file->f_dentry->d_inode;
        atomic_inc (&inode->i_count);
        break;

      case KERFS_DISTRIBUTED_FILE:
      case KERFS_REPLICATED_FILE:
        if (S_ISDIR (dir_entry->mode))
          ctnr = create_dir_container (dentry, mnt, 1, 0, 0, current->fsuid,
                                       current->fsgid);
        else
          ctnr = create_file_container (dentry, mnt, 1, dir_entry->linked_node,
                                        0, dir_entry->mode, current->fsuid,
                                        current->fsgid);

        if (IS_ERR (ctnr))
          goto done;

        inode = get_file_ctnr_inode (ctnr);

        if (inode)
          dir_entry->ctnrid = ctnr->ctnrid;
        break;

      default:
        PANIC ("Invalid file map type\n");
        break;
    }

done:
  return inode;
}



/** File system dependent lookup function called by the Linux VFS.
 *  @author Renaud Lottiaux
 *
 *  @param dir       Directory to lookup in.
 *  @param dentry    Dentry of the file to lookup for.
 *
 *  @return The dentry of the file to search if found. Null otherwise.
 */
static struct dentry *kerfs_lookup (struct inode *dir,
                                    struct dentry *dentry,
                                    struct nameidata *nd)
{
  container_t *dir_ctnr = dir->i_mapping->a_ctnr;
  struct kerfs_dir_entry *dir_entry;
  struct inode *inode = NULL;
  container_t *ctnr;
  void *err = NULL;

  DEBUG (2, "KERFS : lookup for file %s\n", dentry->d_name.name);

  if (dir_ctnr == NULL)
    return ERR_PTR (-ENOENT);

  if (dentry->d_name.len > KERFS_NAME_LEN)
    return ERR_PTR (-ENAMETOOLONG);

  /* Get the directory inode struct and lock it on the node */

  /* Grab is needed since we use the dirty_compatibility_lookup which
   * can create and add a new file in the directory...
   */

  grab_krg_inode_struct (dir);

  /* Lookup for the file in the kerfs given directory */

  dir_entry = kerfs_find_grab_dir_entry_by_name (dir,
                                                 (char *) dentry->d_name.name);

  if (dir_entry)
    {
      /* Ok, the entry exist */

      if (dir_entry->ctnrid == CTNR_UNUSED)
        /* The entry have not been opened. Let's open it... */
        inode = do_kerfs_lookup (dir, dentry, dir_entry);
      else
        /* The entry have been remotly opened, just create a local inode */
        {
          ctnr = find_container (dir_entry->ctnrid);
          if (!ctnr)
            WARNING ("Cannot find container %ld\n", dir_entry->ctnrid);
          else
            inode = get_file_ctnr_inode (ctnr);
        }

      if (inode != NULL)
        {
          get_krg_inode_struct (inode);
          put_krg_inode_struct (inode);
        }
      else
        err = ERR_PTR (-EIO);

      kerfs_put_dir_entry (dir_entry);
    }

  put_krg_inode_struct (dir);

  if (inode)
    {
      atomic_inc (&inode->i_count);
      d_add (dentry, inode);
      dentry->d_op = &kerfs_dentry_operations;
    }

  if (inode)
    DEBUG (2,
           "KERFS : lookup : done : inode %ld - size %d - mode : %o - uid : %d - gid : %d\n",
           inode->i_ino, (int) inode->i_size, inode->i_mode, inode->i_uid,
           inode->i_gid);
  else
    DEBUG (2, "KERFS : lookup : done : no inode found\n");

  return err;
}



/** File system dependent unlink function called by the Linux VFS.
 *  @author Renaud Lottiaux
 *
 *  @param dir       Directory to unlink a file from.
 *  @param dentry    Dentry of the file to unlink.
 *
 *  @return 0 if everything ok, negative value otherwise.
 */
static int kerfs_unlink (struct inode *dir,
                         struct dentry *dentry)
{
  struct inode *inode = dentry->d_inode;
  int retval = 0;

  DEBUG (2, "KERFS : unlink file %s\n", dentry->d_name.name);

  grab_krg_inode_struct (inode);

  kerfs_delete_dir_entry (dir, inode);

  inode->i_ctime = dir->i_ctime;

  kerfs_dec_count (inode);

  /* This is the last unlink. Count = 2 (1 for the link + 1 for ctnr) */

  if (atomic_read (&inode->i_count) == 2)
    atomic_dec (&inode->i_count);

  mark_inode_dirty (inode);

  put_krg_inode_struct (inode);

  if (atomic_read (&dentry->d_count) == 1)
    d_drop (dentry);
  else
    WARNING ("Hum... Cannot drop dentry for file %s (count %d)\n",
             dentry->d_name.name, atomic_read (&dentry->d_count));

  retval = 0;

  DEBUG (2,
         "KERFS : unlink done : %d links left (inode count %d) (dentry count %d)\n",
         inode->i_nlink, atomic_read (&inode->i_count),
         atomic_read (&dentry->d_count));

  return retval;
}



/** File system dependent symlink function called by the Linux VFS.
 *  @author Renaud Lottiaux
 *
 *  @param dir       Directory to create the symbolic link in.
 *  @param dentry    Dentry of the file to create.
 *  @param symname   Name of the link target.
 *
 *  @return 0 if everything ok, negative value otherwise.
 */
static int kerfs_symlink (struct inode *dir,
                          struct dentry *dentry,
                          const char *symname)
{
  container_t *ctnr;
  struct vfsmount *mnt;
  struct page *page;
  char *page_addr;
  int mode;
  int err;
  unsigned len;

  DEBUG (2, "KERFS : link file %s on %s\n", symname, dentry->d_name.name);

  mode = S_IFLNK | S_IRWXUGO;

  len = strlen (symname) + 1;

  mnt = get_dir_ctnr_mnt (dir);

  ctnr = create_file_container (dentry, mnt, 1, kerrighed_node_id, O_CREAT,
                                mode, current->fsuid, current->fsgid);

  if (IS_ERR (ctnr))
    return PTR_ERR (ctnr);

  err = kerfs_mknod (dir, dentry, mode, ctnr->ctnrid);
  if (err)
    goto exit;

  /* Get the first file page to write the symb link into */
  page = ctnr_grab_object (ctnr->ctnrid, 0);
  page_addr = kmap_atomic (page, KM_USER0);
  set_page_dirty (page);

  /* Write the symb link in the page */

  memcpy (page_addr, symname, len);
  grab_krg_inode_struct (dentry->d_inode);
  dentry->d_inode->i_size = len;
  put_krg_inode_struct (dentry->d_inode);
  update_physical_inode_attr (ctnr, dentry->d_inode, ATTR_SIZE);

  /* Update and unlock the page */

  ctnr_sync_object (ctnr->ctnrid, 0);
  ClearPageDirty (page);

  ctnr_put_object (ctnr->ctnrid, 0);

  kunmap_atomic (page_addr, KM_USER0);

  kerfs_add_dir_entry (dir, (char *) dentry->d_name.name, ctnr,
                       mode, KERFS_DISTRIBUTED_FILE);

exit:

  DEBUG (2, "KERFS : link file %s on %s : done\n", symname,
         dentry->d_name.name);

  return err;
}



/** File system dependent rename function called by the Linux VFS.
 *  @author Renaud Lottiaux
 *
 *  @param old_dir      Directory hosting the old file name.
 *  @param old_dentry   Dentry of the old file name.
 *  @param old_dir      Directory hosting the new file name.
 *  @param old_dentry   Dentry of the new file name.
 *
 *  @return 0 if everything ok, negative value otherwise.
 */
int kerfs_rename (struct inode *old_dir,
                  struct dentry *old_dentry,
                  struct inode *new_dir,
                  struct dentry *new_dentry)
{
  struct inode *old_inode = old_dentry->d_inode;
  struct inode *new_inode = new_dentry->d_inode;
  struct kerfs_dir_entry *old_dir_entry;
  int err = -EPERM;
  umode_t mode;
  int map_type;

  DEBUG (2, "KERFS : move file %s from directory %ld to %s in directory %ld\n",
         old_dentry->d_name.name, old_dir->i_ino,
         new_dentry->d_name.name, new_dir->i_ino);

  /* Lookup for the old file in the kerfs given directory */

  old_dir_entry = kerfs_find_grab_dir_entry_by_name (old_dir,
                                                     (char *) old_dentry->
                                                     d_name.name);

  if (!old_dir_entry)
    {
      err = -ENOENT;
      goto exit;
    }

  mode = old_dir_entry->mode;
  map_type = old_dir_entry->map_type;

  kerfs_put_dir_entry (old_dir_entry);

  grab_krg_inode_struct (old_dir);
  if (old_dir != new_dir)
    grab_krg_inode_struct (new_dir);
  grab_krg_inode_struct (old_inode);

  if (new_inode)
    {
      /* File overwrite case */

      if (S_ISDIR (new_inode->i_mode) && !kerfs_empty_dir (new_inode))
        {
          /* We cannot overwrite a non empty directory */

          err = -ENOTEMPTY;
          goto exit;
        }

      kerfs_delete_dir_entry_no_lock (new_dir, new_inode);
      kerfs_dec_count (new_inode);
    }

  kerfs_inc_count (old_inode);

  kerfs_physical_rename (old_dir->i_ino, new_dir->i_ino,
                         old_dentry->d_inode->i_ino,
                         (char *) new_dentry->d_name.name);

  kerfs_add_dir_entry_no_lock (new_dir, (char *) new_dentry->d_name.name,
                               old_inode->i_mapping->a_ctnr, mode, map_type);

  kerfs_delete_dir_entry_no_lock (old_dir, old_inode);

  kerfs_dec_count (old_inode);

  err = 0;

exit:
  put_krg_inode_struct (old_inode);
  put_krg_inode_struct (old_dir);
  if (old_dir != new_dir)
    put_krg_inode_struct (new_dir);

  DEBUG (2, "KERFS : move file %s : done with error %d\n",
         old_dentry->d_name.name, err);

  return err;
}



/** File system dependent readlink function called by the Linux VFS.
 *  @author Renaud Lottiaux
 *
 *  @param dentry   Dentry of the symbolic link to read.
 *  @param buffer   Buffer to store read data.
 *  @param buflen   Size of the buffer.
 *
 *  @return 0 if everything ok, negative value otherwise.
 */
static int kerfs_readlink (struct dentry *dentry,
                           char *buffer,
                           int buflen)
{
  container_t *ctnr = dentry->d_inode->i_mapping->a_ctnr;
  struct page *page;
  char *page_addr;
  int res;

  DEBUG (2, "KERFS : readlink for file %s\n", dentry->d_name.name);

  page = ctnr_get_object (ctnr->ctnrid, 0);
  page_addr = kmap_atomic (page, KM_USER0);

  res = vfs_readlink (dentry, buffer, buflen, page_addr);

  ctnr_put_object (ctnr->ctnrid, 0);
  kunmap_atomic (page_addr, KM_USER0);

  DEBUG (2, "KERFS : readlink for file %s : done\n", dentry->d_name.name);

  return res;
}



static int kerfs_follow_link (struct dentry *dentry,
                              struct nameidata *nd)
{
  container_t *ctnr = dentry->d_inode->i_mapping->a_ctnr;
  struct page *page;
  char *page_addr;
  int res;

  DEBUG (2, "KERFS : follow link for file %s\n", dentry->d_name.name);

  page = ctnr_get_object (ctnr->ctnrid, 0);
  page_addr = kmap_atomic (page, KM_USER0);

  res = vfs_follow_link (nd, page_addr);

  ctnr_put_object (ctnr->ctnrid, 0);
  kunmap_atomic (page_addr, KM_USER0);

  DEBUG (2, "KERFS : follow link for file %s : done\n", dentry->d_name.name);

  return res;
}



/** File system dependent mkdir function called by the Linux VFS.
 *  @author Renaud Lottiaux
 *
 *  @param dir      Directory to create the directory in.
 *  @param dentry   Dentry of the directory to create.
 *  @param mode     Mode of the directory to create.
 */
static int kerfs_mkdir (struct inode *dir,
                        struct dentry *dentry,
                        int mode)
{
  struct vfsmount *mnt;
  container_t *ctnr;
  int err = 0;

  DEBUG (2, "KERFS : mkdir %s\n", dentry->d_name.name);

  mnt = get_dir_ctnr_mnt (dir);

  ctnr = create_dir_container (dentry, mnt, 1, O_CREAT, mode,
                               current->fsuid, current->fsgid);

  if (IS_ERR (ctnr))
    return PTR_ERR (ctnr);

  mode |= S_IFDIR;

  err = kerfs_mknod (dir, dentry, mode, ctnr->ctnrid);
  if (err == 0)
    kerfs_add_dir_entry (dir, (char *) dentry->d_name.name, ctnr,
                         mode, KERFS_REPLICATED_FILE);

  DEBUG (2, "KERFS : mkdir %s : done (ctnr %ld) (dentry count %d) with "
         "err %d\n", dentry->d_name.name, ctnr->ctnrid,
         atomic_read (&dentry->d_count), err);

  return err;
}



/** File system dependent rmdir function called by the Linux VFS.
 *  @author Renaud Lottiaux
 *
 *  @param dir      Directory to remove the directory from.
 *  @param dentry   Dentry of the directory to remove.
 */
int kerfs_rmdir (struct inode *dir,
                 struct dentry *dentry)
{
  struct inode *inode = dentry->d_inode;
  int retval = -ENOTEMPTY;

  DEBUG (2, "KERFS : remove directory %s\n", dentry->d_name.name);

  get_krg_inode_struct (inode);

  if (kerfs_empty_dir (inode))
    {
      put_krg_inode_struct (inode);
      retval = kerfs_unlink (dir, dentry);
    }
  else
    put_krg_inode_struct (inode);

  DEBUG (2, "KERFS : rmdir done with err %d (dentry count %d)\n", retval,
         atomic_read (&dentry->d_count));

  return retval;
}



/** File system dependent setattr function called by the Linux VFS.
 *  @author Renaud Lottiaux
 *
 *  @param dentry    Dentry to set the attributes for.
 *  @param attr      Attributes to set to the given dentry.
 */
int kerfs_setattr (struct dentry *dentry,
                   struct iattr *attr)
{
  struct inode *inode = dentry->d_inode;
  unsigned int ia_valid = attr->ia_valid;
  int error;

  DEBUG (2, "KERFS : setattr for file %s\n", dentry->d_name.name);

  error = inode_change_ok (inode, attr);
  if (!error)
    {
      if ((ia_valid & ATTR_UID && attr->ia_uid != inode->i_uid) ||
          (ia_valid & ATTR_GID && attr->ia_gid != inode->i_gid))
        error = DQUOT_TRANSFER (inode, attr) ? -EDQUOT : 0;

      if (!error)
        {
          grab_krg_inode_struct (inode);
          error = inode_setattr (inode, attr);
          if (error)
            PANIC ("Don't know what to do yet\n");
          else
            {
              put_krg_inode_struct (inode);
              //        update_physical_inode_attr(inode->i_mapping->a_ctnr, inode);
            }
        }
    }

  DEBUG (2, "KERFS : setattr for file %s : done\n", dentry->d_name.name);

  return error;
}



int kerfs_getattr(struct vfsmount *mnt,
                  struct dentry *dentry,
                  struct kstat *stat)
{
  struct inode *inode = dentry->d_inode;

  DEBUG (2, "KERFS : getattr for file %s\n", dentry->d_name.name);
  
  get_krg_inode_struct (inode);  
  generic_fillattr(inode, stat);
  put_krg_inode_struct (inode);

  DEBUG (2, "KERFS : getattr for file %s done\n",
         dentry->d_name.name);

  return 0;
}



struct inode_operations kerfs_file_inode_operations = {
  setattr: kerfs_setattr,
  getattr: kerfs_getattr,
};


struct inode_operations kerfs_link_inode_operations = {
  readlink:    kerfs_readlink,
  follow_link: kerfs_follow_link,
};


struct inode_operations kerfs_dir_inode_operations = {
  create:  kerfs_create,
  lookup:  kerfs_lookup,
  unlink:  kerfs_unlink,
  symlink: kerfs_symlink,
  mkdir:   kerfs_mkdir,
  rmdir:   kerfs_rmdir,
  rename:  kerfs_rename,
  setattr: kerfs_setattr,
  getattr: kerfs_getattr,
};
