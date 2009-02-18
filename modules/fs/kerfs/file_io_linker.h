/** Container File linker.
 *  @file file_io_linker.h
 *
 *  Link containers and Linux files.
 *  @author Renaud Lottiaux
 */

#ifndef __FILE_LINKER__
#define __FILE_LINKER__


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



typedef struct {
  char file_name[1024];
  struct file *physical_file;
  struct file *physical_dir;
  struct inode *logical_inode;
  umode_t mode;
  int flags;
  off_t size;
  uid_t fsuid;
  gid_t fsgid;
} file_iolinker_data_t;

typedef struct ctnr_file_io_msg {
  ctnrid_t ctnrid;
  struct iattr attrs;
} ctnr_file_io_msg_t;

typedef struct kerfs_rename_msg {
  ctnrid_t old_dir;
  ctnrid_t new_dir;
  ctnrid_t file;
  char new_name;
} kerfs_rename_msg_t;



/*--------------------------------------------------------------------------*
 *                                                                          *
 *                            EXTERN VARIABLES                              *
 *                                                                          *
 *--------------------------------------------------------------------------*/



extern struct iolinker_struct file_io_linker;



/*--------------------------------------------------------------------------*
 *                                                                          *
 *                              EXTERN FUNCTIONS                            *
 *                                                                          *
 *--------------------------------------------------------------------------*/


container_t *create_container_from_file (struct file *file);

container_t *create_file_container (struct dentry *dentry,
                                    struct vfsmount *mnt,
                                    int in_ctnrfs,
                                    kerrighed_node_t linked_node,
                                    int flags, int mode, uid_t uid, gid_t gid);

int update_physical_inode_attr (container_t * ctnr,
                                struct inode *inode, unsigned int ia_valid);

void file_container_init (void);
void file_container_finalize (void);

static inline struct vfsmount *get_dir_ctnr_mnt (struct inode *dir)
{
  file_iolinker_data_t *file_data;

  BUG_ON (dir == NULL);
  BUG_ON (dir->i_mapping == NULL);
  BUG_ON (dir->i_mapping->a_ctnr == NULL);

  file_data = dir->i_mapping->a_ctnr->iolinker_data;

  BUG_ON (file_data == NULL);

  return file_data->physical_file->f_vfsmnt;
}



static inline struct inode *get_file_ctnr_inode (container_t * ctnr)
{
  file_iolinker_data_t *file_data;

  file_data = ctnr->iolinker_data;

  BUG_ON (file_data == NULL);

  return file_data->logical_inode;
}

int kerfs_physical_rename (ctnrid_t old_dir,
                           ctnrid_t new_dir, ctnrid_t file, char *new_name);

#endif
