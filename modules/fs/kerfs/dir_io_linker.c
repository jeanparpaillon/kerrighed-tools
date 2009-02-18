/** Container dir IO linker.
 *  @file dir_io_linker.c
 *  
 *  @author Renaud Lottiaux 
 */

#include <linux/slab.h>
#include <linux/file.h>
#include <linux/pagemap.h>

#include "debug_kerfs.h"

#define MODULE_NAME "Dir IO linker   "

#ifdef DIR_IO_LINKER_DEBUG
#define DEBUG_THIS_MODULE
#endif

#include <tools/kerrighed.h>
#include <ctnr/container_mgr.h>
#include <ctnr/container.h>
#include "dir_io_linker.h"
#include "file_io_linker.h"
#include "physical_fs.h"
#include "dir.h"
#include "inode.h"
#include "super.h"



/*****************************************************************************/
/*                                                                           */
/*                           DIR CONTAINER CREATION                          */
/*                                                                           */
/*****************************************************************************/



/** Instantiate a container with a file linker.
 *  @author Renaud Lottiaux
 *
 *  @param ctnr          Container to instantiate
 *  @param private_data  File to link to the container.
 *
 *  @return error code or 0 if everything was ok.
 */
int dir_instantiate (container_t * ctnr,
                     void *private_data,
                     int master)
{
  file_iolinker_data_t *dir_ctnr_data;
  struct inode *inode, *phys_dir_inode;
  char meta_data_file_name[1024];
  struct file *meta_data_file, *phys_dir_file;
  int r = 0;

  /* Allocate and copy IO linker data */

  dir_ctnr_data = kmalloc (sizeof (file_iolinker_data_t), GFP_KERNEL);

  memcpy (dir_ctnr_data, private_data, sizeof (file_iolinker_data_t));

  DEBUG (2, "Instantiate ctnr %ld with dir : %s\n", ctnr->ctnrid,
         dir_ctnr_data->file_name);

  /* Try to open the given directory */

  DEBUG (3, "Try to open physical dir : %s\n", dir_ctnr_data->file_name);

  phys_dir_file = open_physical_file (dir_ctnr_data->file_name,
                                      O_RDONLY, 0,
                                      dir_ctnr_data->fsuid,
                                      dir_ctnr_data->fsgid);
  if (IS_ERR (phys_dir_file))
    {
      /* Hum hum... Probem when opening the directory... */

      if ((PTR_ERR (phys_dir_file) == -ENOENT) &&
          (dir_ctnr_data->flags & O_CREAT))
        {
          /* If the directory does not exist there's no problem if
           * we have to create it... Just do it !
           */

          DEBUG (3, "Create physical dir : %s\n", dir_ctnr_data->file_name);

          r = create_physical_dir (dir_ctnr_data->file_name,
                                   dir_ctnr_data->mode,
                                   dir_ctnr_data->fsuid, dir_ctnr_data->fsgid);

          if (r)
            {
              phys_dir_file = ERR_PTR (r);
              goto exit_err2;
            }

          phys_dir_file = open_physical_file (dir_ctnr_data->file_name,
                                              O_RDONLY, 0,
                                              dir_ctnr_data->fsuid,
                                              dir_ctnr_data->fsgid);
          if (IS_ERR (phys_dir_file))
            goto exit_err2;
        }
      else
        goto exit_err2;
    }

  phys_dir_inode = phys_dir_file->f_dentry->d_inode;


  /* Ok, we have a valid physical directory.
   * Try to open a physical directory KERFS meta-data file.
   */
  snprintf (meta_data_file_name, 1024, "%s/...", dir_ctnr_data->file_name);

  DEBUG (3, "Try to open physical dir meta-data : %s\n", meta_data_file_name);

  meta_data_file = open_physical_file (meta_data_file_name, O_RDWR, 0,
                                       phys_dir_inode->i_uid,
                                       phys_dir_inode->i_gid);


  if (IS_ERR (meta_data_file))
    {
      /* Hum hum... Probem when opening directory meta-data file... */

      if ((PTR_ERR (meta_data_file) == -ENOENT) &&
          (dir_ctnr_data->flags & O_CREAT))
        {
          /* If the directory does not exist there's no problem if
           * we have to create it... Just do it !
           */
        create_anyway:
          DEBUG (3, "Create physical dir meta-data : %s with mode %o\n",
                 meta_data_file_name, phys_dir_inode->i_mode & (~S_IFDIR));

          meta_data_file = open_physical_file (meta_data_file_name,
                                               O_RDWR | O_CREAT,
                                               phys_dir_inode->
                                               i_mode & (~S_IFDIR),
                                               phys_dir_inode->i_uid,
                                               phys_dir_inode->i_gid);

          if (IS_ERR (meta_data_file))
            goto exit_err;
        }
      else
        {
          WARNING ("Hum, the meta data file does not exist... "
                   "Create it anyway\n");
          goto create_anyway;
        }
    }
  else
    {
      /* Great, the meta-data file already exist ! Nothing to do... */
    }

  inode = meta_data_file->f_dentry->d_inode;

  dir_ctnr_data->physical_file = meta_data_file;
  dir_ctnr_data->physical_dir = phys_dir_file;

  inode->i_objid = ctnr->ctnrid;

  /* Create logical inode and mapping hosting the container pages */

  dir_ctnr_data->logical_inode = kerfs_get_inode (kerfs_sb,
                                                  phys_dir_inode->i_mode, ctnr);

  /* Init the global inode with physical file inode informations */
  if (master)
    {
      int orig_mode;

      orig_mode = inode->i_mode;
      inode->i_mode = phys_dir_inode->i_mode;
      kerfs_init_krg_inode_struct (ctnr->ctnrid, inode);
      inode->i_mode = orig_mode;
    }

  increment_container_usage (ctnr);

  if (inode)
    {
      if (!(dir_ctnr_data->logical_inode->i_state & I_DIRTY)
          && (inode->i_state & I_DIRTY) && master)
        __mark_inode_dirty (dir_ctnr_data->logical_inode,
                            inode->i_state & I_DIRTY);
      kerfs_copy_inode (dir_ctnr_data->logical_inode, inode);
    }

  ctnr->iolinker_data = dir_ctnr_data;

  DEBUG (2, "Instantiate ctnr %ld with dir : %s done with err %d\n",
         ctnr->ctnrid, dir_ctnr_data->file_name, r);

  return r;

exit_err2:
  meta_data_file = phys_dir_file;

exit_err:
  /* There is definitely a problem... */

  DEBUG (2, "Instantiate ctnr %ld with dir : %s done with err %ld\n",
         ctnr->ctnrid, dir_ctnr_data->file_name, PTR_ERR (meta_data_file));

  kfree (dir_ctnr_data);
  return PTR_ERR (meta_data_file);
}



void d_drop_aliases (struct inode *inode)
{
  struct list_head *tmp, *head = &inode->i_dentry;

  spin_lock (&dcache_lock);
  tmp = head;
  while ((tmp = tmp->next) != head)
    {
      struct dentry *dentry = list_entry (tmp, struct dentry, d_alias);

      d_drop (dentry);
    }
  spin_unlock (&dcache_lock);
}



/** Uninstantiate a directory container.
 *  @author Renaud Lottiaux
 *
 *  @param ctnr          Container to uninstantiate
 */
void dir_uninstantiate (container_t * ctnr,
                        int destroy)
{
  file_iolinker_data_t *file_data;

  file_data = (file_iolinker_data_t *) ctnr->iolinker_data;

  if (file_data == NULL)
    return;

  if (file_data->physical_file)
    {

      if (destroy)
        {
          remove_physical_file (file_data->physical_file);
          remove_physical_dir (file_data->physical_dir);
        }
      else
        {
          close_physical_file (file_data->physical_file);
          close_physical_file (file_data->physical_dir);
        }
    }

  if (file_data->logical_inode)
    {
      d_drop_aliases (file_data->logical_inode);

      if (atomic_read (&file_data->logical_inode->i_count) != 0)
        iput (file_data->logical_inode);

      file_data->logical_inode->i_mapping->a_ctnr = NULL;
    }

  kfree (file_data);
  ctnr->iolinker_data = NULL;
}



/** Create container from dir
 *  @author Renaud Lottiaux
 *
 *  @param vm_file	struct file to build a container from
 * 
 *  @return		a new container built from the file
 */
container_t *__create_dir_container (ctnrid_t ctnrid,
                                     struct dentry *dentry,
                                     struct vfsmount *mnt,
                                     int in_kerfs,
                                     int flags,
                                     int mode,
                                     uid_t uid,
                                     gid_t gid)
{
  char *tmp = (char *) __get_free_page (GFP_KERNEL), *file_name;
  file_iolinker_data_t *file_data;
  container_t *ctnr = NULL;

  DEBUG (2, "BEGIN\n");

  /* Allocate space for the file_data structure */
  file_data = kmalloc (sizeof (file_iolinker_data_t), GFP_KERNEL);
  ASSERT (file_data != NULL);

  /* Let's find the name of the file ... */
  if (in_kerfs)
    file_name = d_path_from_kerfs (dentry, mnt, tmp, MAX_DIR_IO_PRIVATE_DATA);
  else
    file_name = physical_d_path (dentry, mnt, tmp);
  if (!file_name)
    goto exit;

  /* ... and copy it into file_data */
  strncpy (file_data->file_name, file_name, 1024);

  /* Enpack the directory IO linker data */

  file_data->mode = mode;
  file_data->flags = flags;
  file_data->size = 0;
  file_data->fsuid = uid;
  file_data->fsgid = gid;

  /* Init the global inode with physical file inode informations */

  if (ctnrid == 0)
    ctnrid = alloc_new_ctnr_id ();

  //  kerfs_init_krg_inode_struct(ctnrid, NULL);

  /* file_data is now correctly filled in */
  /* We create the container */
  ctnr = __create_new_container (ctnrid, 20, DIR_LINKER,
                                 CTNR_FULL_LINKED, MAX_DIR_IO_PRIVATE_DATA,
                                 file_data, sizeof (file_iolinker_data_t), 0);

  /* We can free all the memory allocated */
  kfree (file_data);

exit:
  free_page ((unsigned long) tmp);

  return ctnr;
}



/*****************************************************************************/
/*                                                                           */
/*                         DIR CONTAINER IO FUNCTIONS                        */
/*                                                                           */
/*****************************************************************************/



/** Handle a container dir page first touch
 *  @author Renaud Lottiaux
 *
 *  @param  ctnr       Container descriptor
 *  @param  objid     Id of the page to create.
 *  @param  objEntry  Container page descriptor.
 *
 *  @return  0 if everything is ok. Negative value otherwise.
 */
int dir_first_touch (ctnrObj_t * objEntry, 
                     container_t * ctnr,
                     objid_t objid)
{
  struct kerfs_dir_entry *dir_entry;
  int res, i;

  res = file_io_linker.first_touch (objEntry, ctnr, objid);

  if (res < 0)
    return res;

  dir_entry = (struct kerfs_dir_entry *) kmap_atomic (objEntry->object,
                                                      KM_USER0);

  for (i = 0; i < PAGE_SIZE / KERFS_DIRENT_SIZE; i++)
    {
      dir_entry->ctnrid = CTNR_UNUSED;
      dir_entry = next_dir_entry (dir_entry);
    }

  kunmap_atomic (dir_entry, KM_USER0);

  return res;
}



/****************************************************************************/



extern int file_invalidate_page (ctnrObj_t * objEntry,
                                 container_t * ctnr,
                                 objid_t objid);

extern int file_sync_page (ctnrObj_t * objEntry,
                           container_t * ctnr,
                           objid_t objid,
                           void *object);

extern int file_remove_page (ctnrObj_t * objEntry,
                             container_t * ctnr,
                             objid_t objid);

extern void *file_alloc_object (ctnrObj_t * objEntry,
                                container_t * ctnr,
                                objid_t objid);

extern int file_import_object (ctnrObj_t * objEntry,
                               char *buffer, struct gimli_desc *desc);

extern int file_export_object (char *buffer, struct gimli_desc *desc,
                               ctnrObj_t * objEntry);

extern int file_insert_page (ctnrObj_t * objEntry,
                             container_t * ctnr,
                             objid_t objid);

/* Init the dir IO linker */

struct iolinker_struct dir_io_linker = {
  instantiate:       dir_instantiate,
  uninstantiate:     dir_uninstantiate,
  first_touch:       dir_first_touch,
  insert_object:     file_insert_page,
  remove_object:     file_remove_page,
  invalidate_object: file_invalidate_page,
  sync_object:       file_sync_page,
  linker_name:       "dir",
  linker_id:         DIR_LINKER,
  alloc_object:      file_alloc_object,
  export_object:     file_export_object,
  import_object:     file_import_object
};
