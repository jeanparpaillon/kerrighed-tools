/** Access to Physical File System management.
 *  @file physical_fs.h
 *  
 *  @author Renaud Lottiaux 
 */

#ifndef __PHYSICAL_FS__
#define __PHYSICAL_FS__



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



/*--------------------------------------------------------------------------*
 *                                                                          *
 *                              EXTERN FUNCTIONS                            *
 *                                                                          *
 *--------------------------------------------------------------------------*/


char *physical_d_path (struct dentry *dentry, struct vfsmount *mnt, char *tmp);

struct file *open_physical_file (char *filename,
                                 int flags, int mode, uid_t uid, gid_t gid);

int close_physical_file (struct file *file);

int remove_physical_file (struct file *file);

int remove_physical_dir (struct file *file);

#endif // __PHYSICAL_FS__
