/** Container File System Super Block Management.
 *  @file super.h
 *  
 *  @author Renaud Lottiaux 
 */

#ifndef __KERFS_SUPER__
#define __KERFS_SUPER__



/*--------------------------------------------------------------------------*
 *                                                                          *
 *                                 MACROS                                   *
 *                                                                          *
 *--------------------------------------------------------------------------*/



#define KERFS_SUPER_MAGIC 0x3542



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



extern struct dentry *kerfs_root;
extern struct vfsmount *kerfs_mnt;
extern struct dentry *physical_root;
extern struct vfsmount *physical_mnt;
extern struct super_block *kerfs_sb;
extern char kerfs_phys_root_name[];



/*--------------------------------------------------------------------------*
 *                                                                          *
 *                              EXTERN FUNCTIONS                            *
 *                                                                          *
 *--------------------------------------------------------------------------*/



int kerfs_init (void);

int kerfs_finalize (void);



#endif // __KERFS_SUPER__
