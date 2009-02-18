/** Container File System inode management.
 *  @file inode.h
 *  
 *  @author Renaud Lottiaux 
 */

#ifndef __KERFS_INODE__
#define __KERFS_INODE__

#include <ctnr/container.h>


/*--------------------------------------------------------------------------*
 *                                                                          *
 *                                 MACROS                                   *
 *                                                                          *
 *--------------------------------------------------------------------------*/



/*--------------------------------------------------------------------------*
 *                                                                          *
 *                                  TYPES                                   *
 *                                                                          *
 *--------------------------------------------------------------------------*/



/*--------------------------------------------------------------------------*
 *                                                                          *
 *                            EXTERN VARIABLES                              *
 *                                                                          *
 *--------------------------------------------------------------------------*/



extern struct inode_operations kerfs_dir_inode_operations;
extern struct inode_operations kerfs_file_inode_operations;
extern struct inode_operations kerfs_link_inode_operations;

/* Container of inode structures */
extern container_t *inode_struct_ctnr;



/*--------------------------------------------------------------------------*
 *                                                                          *
 *                              EXTERN FUNCTIONS                            *
 *                                                                          *
 *--------------------------------------------------------------------------*/



static inline void get_krg_inode_struct (struct inode *inode)
{
  ctnr_get_object (INODE_STRUCT_CTNR_ID, inode->i_objid);
}

static inline void put_krg_inode_struct (struct inode *inode)
{
  ctnr_put_object (INODE_STRUCT_CTNR_ID, inode->i_objid);
}

static inline void grab_krg_inode_struct (struct inode *inode)
{
  ctnr_grab_object (INODE_STRUCT_CTNR_ID, inode->i_objid);
}



void kerfs_init_krg_inode_struct (ctnrid_t ctnrid, struct inode *inode);

struct inode *kerfs_get_inode (struct super_block *sb,
                               int mode, container_t * ctnr);

int kerfs_mknod (struct inode *dir, struct dentry *dentry, int mode, int dev);


static inline void kerfs_copy_inode (struct inode *dest, struct inode *src)
{
  dest->i_size = src->i_size;
  dest->i_mode = (dest->i_mode & S_IFMT) | (src->i_mode & (~S_IFMT));
  dest->i_uid = src->i_uid;
  dest->i_gid = src->i_gid;
  dest->i_atime = src->i_atime;
  dest->i_mtime = src->i_mtime;
  dest->i_ctime = src->i_ctime;
  dest->i_version = src->i_version;
  dest->i_state = src->i_state;
  dest->i_nlink = src->i_nlink;
}


#endif // __KERFS_INODE__
