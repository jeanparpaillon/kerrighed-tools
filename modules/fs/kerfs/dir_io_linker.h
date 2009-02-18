/** Container Dir linker.
 *  @file dir_io_linker.h
 *
 *  Link containers and Linux directories.
 *  @author Renaud Lottiaux
 */

#ifndef __DIR_LINKER__
#define __DIR_LINKER__



/*--------------------------------------------------------------------------*
 *                                                                          *
 *                                 MACROS                                   *
 *                                                                          *
 *--------------------------------------------------------------------------*/



#define MAX_DIR_IO_PRIVATE_DATA 2048



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



extern struct iolinker_struct dir_io_linker;



/*--------------------------------------------------------------------------*
 *                                                                          *
 *                              EXTERN FUNCTIONS                            *
 *                                                                          *
 *--------------------------------------------------------------------------*/



container_t *__create_dir_container (ctnrid_t ctnrid,
                                     struct dentry *dentry,
                                     struct vfsmount *mnt,
                                     int in_ctnrfs, int flags,
                                     int mode, uid_t uid, gid_t gid);

static inline container_t *create_dir_container (struct dentry *dentry,
                                                 struct vfsmount *mnt,
                                                 int in_ctnrfs,
                                                 int flags, int mode,
                                                 uid_t uid, gid_t gid)
{
  return __create_dir_container (0, dentry, mnt, in_ctnrfs, flags, mode, uid,
                                 gid);
}

#endif // __DIR_LINKER__
