/** Container File System super block management.
 *  @file super.c
 *  
 *  @author Renaud Lottiaux 
 */

#include <linux/fs.h>
#include <linux/file.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/pagemap.h>
#include <linux/statfs.h>

#include "debug_kerfs.h"

#define MODULE_NAME "Ctnr FS Super   "

#ifdef KERFS_SUPER_DEBUG
#define DEBUG_THIS_MODULE
#endif

#include <ctnr/container_mgr.h>
#include "file.h"
#include "address_space.h"
#include "dir.h"
#include "physical_fs.h"
#include "dir_io_linker.h"
#include "file_io_linker.h"
#include "super.h"
#include "inode.h"


struct dentry *kerfs_root = NULL;
struct vfsmount *kerfs_mnt = NULL;

struct dentry *physical_root = NULL;
struct vfsmount *physical_mnt = NULL;

struct super_block *kerfs_sb = NULL;

container_t *kerfs_root_ctnr = NULL;


char kerfs_phys_root_name[] = "/.KERFS_ROOT/";


static int kerfs_statfs (struct super_block *sb,
                         struct kstatfs *buf)
{
  DEBUG (2, "KERFS : statfs\n");

  buf->f_type = KERFS_SUPER_MAGIC;
  buf->f_bsize = PAGE_CACHE_SIZE;
  buf->f_bfree = 0;
  buf->f_bavail = 0;
  buf->f_ffree = 0;
  buf->f_namelen = KERFS_NAME_LEN;

  DEBUG (2, "KERFS : statfs : done\n");

  return 0;
}



static void kerfs_read_inode (struct inode *inode)
{
  DEBUG (2, "KERFS : Read Inode\n");

  inode->i_fop = &kerfs_file_fops;
  inode->i_mapping->a_ops = &kerfs_aops;

  /* Should increment container usage I suppose... */

  DEBUG (2, "KERFS : Read Inode : done\n");
}



static int kerfs_write_inode (struct inode *inode,
                              int sync)
{
  container_t *ctnr = inode->i_mapping->a_ctnr;

  if (ctnr == NULL)
    return -EINVAL;

  DEBUG (2, "KERFS : Write Inode\n");

  update_physical_inode_attr (ctnr, inode, ATTR_SIZE | ATTR_UID | ATTR_GID
                              | ATTR_MODE);

  DEBUG (2, "KERFS : Write Inode : done\n");

  return 0;
}



static void kerfs_delete_inode (struct inode *inode)
{
  container_t *ctnr;

  DEBUG (2, "KERFS : Delete Inode %ld (state 0x%08lx)\n", inode->i_ino,
         inode->i_state);

  ASSERT (inode != NULL);
  ASSERT (inode->i_mapping != NULL);

  ctnr = inode->i_mapping->a_ctnr;

  if (ctnr != NULL)
    {
      if (!(ctnr->flags & CTNR_FREEING))
        destroy_container (ctnr);
    }
  else
    WARNING ("Hum... Container has probably already been destroyed...\n");

  clear_inode (inode);

  DEBUG (2, "KERFS : Delete Inode %ld : done\n", inode->i_ino);
}



static struct super_operations kerfs_sops = {
  read_inode:   kerfs_read_inode,
  write_inode:  kerfs_write_inode,
  delete_inode: kerfs_delete_inode,
  statfs:       kerfs_statfs,
};


static int kerfs_fill_super (struct super_block *sb,
                             void *data,
                             int silent)
{
  struct file *file;
  struct inode *root_inode;
  struct dentry *root_dentry;
  file_iolinker_data_t *file_data;

  DEBUG (2, "KERFS : Fill Super\n");

  sb->s_maxbytes = MAX_LFS_FILESIZE;
  sb->s_blocksize = PAGE_CACHE_SIZE;
  sb->s_blocksize_bits = PAGE_CACHE_SHIFT;
  sb->s_magic = KERFS_SUPER_MAGIC;
  sb->s_op = &kerfs_sops;
  sb->s_time_gran = 1;

  kerfs_sb = sb;

  /* Look up for the KerFS root container */
  
  kerfs_root_ctnr = find_container (KERFS_ROOT_CTNR_ID);
  
  if (kerfs_root_ctnr != NULL)
    goto found_root;
  
  if (kerrighed_node_id == 0)
    {
      /* Create the kerfs root directory container */

      DEBUG (2, "Create CTNR FS Root container\n");  

try_again:
      file = open_physical_file (kerfs_phys_root_name, O_RDONLY, 0644, 0, 0);

      if (IS_ERR (file))
        {
          if (PTR_ERR (file) == -ENOENT)
            {
              create_physical_dir (kerfs_phys_root_name, 0777, 0, 0);
              goto try_again;
            }
          else
            return PTR_ERR (file);
        }
      
      kerfs_root_ctnr = __create_dir_container (KERFS_ROOT_CTNR_ID,
                                               file->f_dentry,
                                               file->f_vfsmnt, 0,
                                               O_CREAT,
                                               file->f_dentry->d_inode->i_mode,
                                               0, 0);

      DEBUG (2, "Kerfs Root container created at %p\n", kerfs_root_ctnr);
      
      if (IS_ERR (kerfs_root_ctnr))
        return PTR_ERR (kerfs_root_ctnr);
    }
  else
    {
      DEBUG (2, "Wait for Kerfs Root container creation by node 0\n");

      while (kerfs_root_ctnr == NULL)
        {
          set_current_state(TASK_INTERRUPTIBLE) ;
          kerfs_root_ctnr = find_container (KERFS_ROOT_CTNR_ID);
        }
    }
    
found_root:

  file_data = kerfs_root_ctnr->iolinker_data;
  ASSERT (file_data != NULL);
  root_inode = file_data->logical_inode;
  ASSERT (root_inode != NULL);

  root_dentry = d_alloc_root (root_inode);

  if (!root_dentry)
    {
      iput (root_inode);
      return -ENOMEM;
    }

  sb->s_root = root_dentry;

  kerfs_root = root_dentry;

  DEBUG (2, "KERFS : Fill Super : done\n");

  return 0;
}

struct super_block *kerfs_get_sb (struct file_system_type *fs_type,
                                  int flags,
                                  const char *dev_name,
                                  void *data)
{
  struct super_block *sb;

  DEBUG (2, "KERFS : Get SB\n");

  if (kerfs_sb != NULL)
    {
      kerfs_sb->s_count++ ;
      return kerfs_sb ;
    }
  else
    sb = get_sb_nodev (fs_type, flags, data, kerfs_fill_super);

  DEBUG (2, "KERFS : Get SB : done (%p)\n", sb);

  return sb;
}


static struct file_system_type kerfs_fs_type = {
  .name = "kerfs",
  .get_sb = kerfs_get_sb,
  .kill_sb = kill_anon_super,
};


int kerfs_init ()
{
  int err;

  return 0;

  printk ("Init Container File System\n");

  init_unique_id_root (&file_struct_unique_id_root);

  kerfs_mnt = init_task.fs->rootmnt;

  /* Create CTNR File System basic containers */

  if (kerrighed_node_id == 0)
    {
      file_struct_ctnr = __create_new_container (FILE_STRUCT_CTNR_ID, 32,
                                                 FILE_STRUCT_LINKER,
                                                 CTNR_NOT_LINKED,
                                                 sizeof (struct file),
                                                 NULL, 0, 0);

      if (IS_ERR (file_struct_ctnr))
        return PTR_ERR (file_struct_ctnr);
      
      inode_struct_ctnr = __create_new_container (INODE_STRUCT_CTNR_ID, 32,
                                                  INODE_LINKER,
                                                  CTNR_NOT_LINKED,
                                                  sizeof (struct inode),
                                                  NULL, 0, 0);

      if (IS_ERR (inode_struct_ctnr))
        return PTR_ERR (inode_struct_ctnr);
    }
  else
    {
      file_struct_ctnr = find_container (FILE_STRUCT_CTNR_ID);

      while (file_struct_ctnr == NULL)
        {
          set_current_state(TASK_INTERRUPTIBLE) ;
          file_struct_ctnr = find_container (FILE_STRUCT_CTNR_ID);
        }

      inode_struct_ctnr = find_container (INODE_STRUCT_CTNR_ID);

      while (inode_struct_ctnr == NULL)
        {
          set_current_state(TASK_INTERRUPTIBLE) ;
          inode_struct_ctnr = find_container (INODE_STRUCT_CTNR_ID);
        }
    }

  /* Register and mount the CTNR File System */

  err = register_filesystem (&kerfs_fs_type);
  kerfs_mnt = kern_mount (&kerfs_fs_type);

  if (err)
    return err;

  //  scan_name_space();

  printk ("Init Container File System : done\n");

  return 0;
}


int kerfs_finalize ()
{
  printk ("Finalize Container File System\n");

  return 0;
}
