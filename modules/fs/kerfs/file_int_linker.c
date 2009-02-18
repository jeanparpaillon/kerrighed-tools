/** Container file interface linker.
 *  @file file_int_linker.c
 *  
 *  @author Renaud Lottiaux 
 */

#include <linux/slab.h>
#include <asm/uaccess.h>

#include "debug_kerfs.h"

#define MODULE_NAME "File int linker "

#ifdef FILE_INT_LINKER_DEBUG
#define DEBUG_THIS_MODULE
#endif

#include <ctnr/container_mgr.h>
#include "file_int_linker.h"
#include "file_io_linker.h"
#include "dfs_server.h"
#include "regular_file_mgr.h"
#include "dfs.h"



/*****************************************************************************/
/*                                                                           */
/*                         FILE INTERFACE LINKER CREATION                    */
/*                                                                           */
/*****************************************************************************/



/** Link a REAL file struct to a container */

int link_file_to_ctnr (struct file **_file,
                       struct task_struct *tsk)
{
  regular_file_krg_desc_t *desc;
  struct dfs_operations *ops;
  container_t *ctnr = NULL;
  struct file *file = *_file;
  unsigned short i_mode;
  int desc_size;
  int r = 0;

  ASSERT (file != NULL);
  ASSERT (tsk != NULL);

  if (file->f_flags & O_FAF)
    return 0;

  ASSERT (file->f_dentry != NULL);

  DEBUG (2, "Link file (%p, name %s) to a container\n", file,
         file->f_dentry->d_name.name);

  /* Only link regular files */
  if (!S_ISREG (file->f_dentry->d_inode->i_mode))
    return 0;

  /* Do not link already linked files */
  if (file->f_dentry->d_inode->i_mapping->a_ctnr != NULL)
    return 0;

  /* Do not link files from non standard file systems */
  if (!(file->f_dentry->d_sb &&
        file->f_dentry->d_sb->s_type &&
        (file->f_dentry->d_sb->s_type->fs_flags & FS_REQUIRES_DEV)))
    return 0;

  ctnr = create_file_container (file->f_dentry, file->f_vfsmnt, 0,
                                kerrighed_node_id,
                                file->f_flags, file->f_dentry->d_inode->i_mode,
                                tsk->uid, tsk->gid);

  if (IS_ERR (ctnr))
    return PTR_ERR (ctnr);

  i_mode = file->f_dentry->d_inode->i_mode & S_IFMT;
  ops = get_dfs_ops (i_mode);

  if (ops->get_krg_desc (file, (void **) &desc, &desc_size) != 0)
    r = -EINVAL;
  else if (desc)
    {
      desc->ctnrid = ctnr->ctnrid;

      file = ops->create_file_from_krg_desc (tsk, desc);

      filp_close (*_file, tsk->files);

      *_file = file;

      kfree (desc);
    }
  else
    r = -EINVAL;

  DEBUG (2, "Link file (%p) to a container : done with err %d\n", file, r);

  return r;
}




/* Page Server Initialisation */

void file_int_init (void)
{
  DEBUG (1, "begin init\n");

  DEBUG (1, "init : done\n");
}



/* Page Server Finalization */

void file_int_finalize (void)
{
}
