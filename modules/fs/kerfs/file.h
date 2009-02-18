/** Container File System file struct management.
 *  @file file.h
 *  
 *  @author Renaud Lottiaux 
 */

#ifndef __KERFS_FILE__
#define __KERFS_FILE__



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



extern struct file_operations kerfs_file_fops;

/* Unique file descriptor id generator root */
extern unique_id_root_t file_struct_unique_id_root;

/* Container of file structures */
extern container_t *file_struct_ctnr;



/*--------------------------------------------------------------------------*
 *                                                                          *
 *                              EXTERN FUNCTIONS                            *
 *                                                                          *
 *--------------------------------------------------------------------------*/

void kerfs_init_krg_file_struct (struct file *filp);

struct file *open_logical_file (char *filename,
                                int flags, int mode, uid_t uid, gid_t gid);

#endif // __KERFS_FILE__
