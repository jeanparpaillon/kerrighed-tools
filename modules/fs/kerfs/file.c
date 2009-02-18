/** Container File System file struct management.
 *  @file file.c
 *  
 *  @author Renaud Lottiaux 
 */

#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/module.h>

#include "debug_kerfs.h"

#define MODULE_NAME "Ctnr FS File    "

#ifdef KERFS_FILE_DEBUG
#define DEBUG_THIS_MODULE
#endif

#include <ctnr/container_mgr.h>
#include <mm/memory_int_linker.h>
#include "inode.h"
#include "super.h"


/* Unique file descriptor id generator root */
unique_id_root_t file_struct_unique_id_root;

/* Container of file structures */
container_t *file_struct_ctnr = NULL;



struct file *open_logical_file (char *filename,
                                int flags,
                                int mode, 
                                uid_t uid,
                                gid_t gid)
{
  struct dentry *saved_root;
  struct vfsmount *saved_mnt;
  struct file *file;

  saved_mnt = current->fs->rootmnt;
  saved_root = current->fs->root;

  current->fs->rootmnt = kerfs_mnt;
  current->fs->root = kerfs_root;
  current->fsuid = uid;
  current->fsgid = gid;

  file = filp_open (filename, flags, mode);

  current->fs->rootmnt = saved_mnt;
  current->fs->root = saved_root;

  return file;
}



/*****************************************************************************/
/*                                                                           */
/*                        FILE STRUCT TOOLS FUNCTIONS                        */
/*                                                                           */
/*****************************************************************************/



void kerfs_copy_file (struct file *dest,
                      struct file *src)
{
  dest->f_pos = src->f_pos;
  dest->f_version = src->f_version;
}

void kerfs_init_krg_file_struct (struct file *filp)
{
  struct page *page;
  struct file *ctnr_file;

  DEBUG (2, "KERFS : init file struct of file %s\n",
         filp->f_dentry->d_name.name);

  filp->f_objid = get_unique_id (&file_struct_unique_id_root);

  /* Dirty fix until container page tables are able to store 2^32 entries */

  filp->f_objid = (filp->f_objid & ((1 << 12) - 1)) | (kerrighed_node_id << 12);

  page = ctnr_grab_object (FILE_STRUCT_CTNR_ID, filp->f_objid);

  ASSERT (page != NULL);

  ctnr_file = (struct file *) kmap_atomic (page, KM_USER0);
  kerfs_copy_file (ctnr_file, filp);
  kunmap_atomic (ctnr_file, KM_USER0);

  ctnr_put_object (FILE_STRUCT_CTNR_ID, filp->f_objid);
}



void get_krg_file_struct (struct file *filp)
{
  struct page *page;
  struct file *ctnr_file;

  DEBUG (2, "KERFS : get file struct of file %s (objid %ld)\n",
         filp->f_dentry->d_name.name, filp->f_objid);

  if (filp->f_objid == 0)
    return;

  page = ctnr_get_object (FILE_STRUCT_CTNR_ID, filp->f_objid);

  ASSERT (page != NULL);

  ctnr_file = (struct file *) kmap_atomic (page, KM_USER0);
  kerfs_copy_file (filp, ctnr_file);
  kunmap_atomic (ctnr_file, KM_USER0);
  ctnr_put_object (FILE_STRUCT_CTNR_ID, filp->f_objid);

  filp->private_data = NULL;

  DEBUG (2, "KERFS : put file struct of file %s (objid %ld) : done page = %p\n",
         filp->f_dentry->d_name.name, filp->f_objid, filp->private_data);
}



void grab_krg_file_struct (struct file *filp)
{
  struct page *page;
  struct file *ctnr_file;

  DEBUG (2, "KERFS : grab file struct of file %s (objid %ld)\n",
         filp->f_dentry->d_name.name, filp->f_objid);

  if (filp->f_objid == 0)
    return;

  page = ctnr_grab_object (FILE_STRUCT_CTNR_ID, filp->f_objid);

  ASSERT (page != NULL);

  ctnr_file = (struct file *) kmap_atomic (page, KM_USER0);
  kerfs_copy_file (filp, ctnr_file);
  kunmap_atomic (ctnr_file, KM_USER0);

  filp->private_data = page;

  DEBUG (2,
         "KERFS : grab file struct of file %s (objid %ld) : done page = %p\n",
         filp->f_dentry->d_name.name, filp->f_objid, filp->private_data);
}



void put_krg_file_struct (struct file *filp)
{
  struct page *page = filp->private_data;
  struct file *ctnr_file;

  DEBUG (2, "KERFS : put file struct of file %s (objid %ld) page = %p\n",
         filp->f_dentry->d_name.name, filp->f_objid, page);

  if (page == NULL)
    return;

  if (filp->f_objid == 0)
    return;

  filp->private_data = NULL;

  ctnr_file = (struct file *) kmap_atomic (page, KM_USER0);
  kerfs_copy_file (ctnr_file, filp);
  kunmap_atomic (ctnr_file, KM_USER0);

  ctnr_put_object (FILE_STRUCT_CTNR_ID, filp->f_objid);

  DEBUG (2, "KERFS : put file struct of file %s (objid %ld) : done\n",
         filp->f_dentry->d_name.name, filp->f_objid);
}



static int kerfs_sync_file (struct file *file,
                            struct dentry *dentry,
                            int datasync)
{
  DEBUG (2, "KERFS : file sync\n");

  return 0;
}



loff_t kerfs_file_llseek (struct file * filp,
                          loff_t offset,
                          int origin)
{
  loff_t r;

  DEBUG (2,
         "KERFS : lseek on file %s at offset %ld (objid %ld) current offset is %ld\n",
         filp->f_dentry->d_name.name, (unsigned long) offset, filp->f_objid,
         (unsigned long) filp->f_pos);

  grab_krg_file_struct (filp);

  r = generic_file_llseek (filp, offset, origin);

  put_krg_file_struct (filp);

  DEBUG (2,
         "KERFS : lseek on file %s (objid %ld) offset is now %ld : done with err %d\n",
         filp->f_dentry->d_name.name, filp->f_objid,
         (unsigned long) filp->f_pos, (unsigned int) r);

  return r;
}



ssize_t kerfs_file_read (struct file * filp,
                         char *buf,
                         size_t count,
                         loff_t * ppos)
{
  ssize_t r;

  DEBUG (2,
         "KERFS : read %zd bytes from file %s at offset %ld (objid %ld) in buffer %p\n",
         count, filp->f_dentry->d_name.name, (unsigned long) *ppos,
         filp->f_objid, buf);

  grab_krg_file_struct (filp);
  get_krg_inode_struct (filp->f_dentry->d_inode);

  r = generic_file_read (filp, buf, count, ppos);

  put_krg_inode_struct (filp->f_dentry->d_inode);
  put_krg_file_struct (filp);

  DEBUG (2, "KERFS : read of file %s at offset %ld (objid %ld) : done\n",
         filp->f_dentry->d_name.name, (unsigned long) *ppos, filp->f_objid);

  return r;
}



ssize_t kerfs_file_write (struct file * filp,
                          const char *buf,
                          size_t count,
                          loff_t * ppos)
{
  ssize_t r;

  DEBUG (2, "KERFS : write %zd bytes to file %s at offset %ld (objid %ld)\n",
         count, filp->f_dentry->d_name.name, (unsigned long) *ppos,
         filp->f_objid);

  grab_krg_file_struct (filp);
  grab_krg_inode_struct (filp->f_dentry->d_inode);

  r = generic_file_write (filp, buf, count, ppos);

  put_krg_inode_struct (filp->f_dentry->d_inode);
  put_krg_file_struct (filp);

  DEBUG (2, "KERFS : write to file %s at offset %ld (objid %ld) : "
         "done with error %zd\n",
         filp->f_dentry->d_name.name, (unsigned long) *ppos, filp->f_objid, r);

  return r;
}



int kerfs_file_mmap (struct file *file,
                     struct vm_area_struct *vma)
{
  container_t *ctnr;
  int r;

  ctnr = file->f_dentry->d_inode->i_mapping->a_ctnr;

  if (ctnr == NULL)
    {
      WARNING ("NULL container for file %s\n", file->f_dentry->d_name.name);
      return -EIO;
    }

  if (vma->vm_flags & VM_SHARED)
    PANIC ("Not managed\n");
  else
    r = generic_file_mmap (file, vma);

  return 0;
}



int kerfs_file_open (struct inode *inode,
                     struct file *filp)
{
  int r;

  DEBUG (2, "KERFS : open file %s - inode %ld\n",
         filp->f_dentry->d_name.name, inode->i_ino);

  get_krg_inode_struct (inode);
  put_krg_inode_struct (inode);

  r = generic_file_open (inode, filp);

  DEBUG (2, "KERFS : open file %s : f_objid %ld - inode %ld : "
         "done with error %d\n",
         filp->f_dentry->d_name.name, filp->f_objid, inode->i_ino, r);

  DEBUG (2, "KERFS : open file %s : mode %o - uid %d - gid %d\n",
         filp->f_dentry->d_name.name, filp->f_dentry->d_inode->i_mode,
         filp->f_dentry->d_inode->i_uid, filp->f_dentry->d_inode->i_gid);

  return r;
}



int kerfs_file_flush (struct file *filp)
{
  DEBUG (1, "-----> Closing file %s (i_size %d) (inode %p)\n",
         filp->f_dentry->d_name.name, (int) filp->f_dentry->d_inode->i_size,
         filp->f_dentry->d_inode);

  return 0;
}



struct file_operations kerfs_file_fops = {
  llseek: kerfs_file_llseek,
  read:   kerfs_file_read,
  write:  kerfs_file_write,
  mmap:   kerfs_file_mmap,
  open:   kerfs_file_open,
  flush:  kerfs_file_flush,
  fsync:  kerfs_sync_file,
};
