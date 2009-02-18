/** Container File System Directory Management.
 *  @file dir.h
 *  
 *  @author Renaud Lottiaux 
 */

#ifndef __KERFS_DIR__
#define __KERFS_DIR__

#include "super.h"



/*--------------------------------------------------------------------------*
 *                                                                          *
 *                                 MACROS                                   *
 *                                                                          *
 *--------------------------------------------------------------------------*/



#define KERFS_NAME_LEN 255
#define KERFS_DIRENT_SIZE 256

#define KERFS_LOCAL_FILE        1
#define KERFS_DISTRIBUTED_FILE  2
#define KERFS_REPLICATED_FILE   3
#define KERFS_MOUNT_FILE        4



/*--------------------------------------------------------------------------*
 *                                                                          *
 *                                  TYPES                                   *
 *                                                                          *
 *--------------------------------------------------------------------------*/



/*
 * Structure of a directory entry
 */
struct kerfs_dir_entry {
  ctnrid_t ctnrid;
  kerrighed_node_t linked_node;
  int map_type;
  umode_t mode;
  int type;
  __u16 rec_len;                /* Directory entry length */
  __u8 name_len;                /* Name length */
  ctnrObj_t *objEntry;          /* Address of the object hosting the dir_entry */
  container_t *ctnr;            /* Container hosting the dir_entry */
  char name[KERFS_NAME_LEN];    /* File name */
};



/*--------------------------------------------------------------------------*
 *                                                                          *
 *                            EXTERN VARIABLES                              *
 *                                                                          *
 *--------------------------------------------------------------------------*/



extern struct file_operations kerfs_dir_fops;



/*--------------------------------------------------------------------------*
 *                                                                          *
 *                              EXTERN FUNCTIONS                            *
 *                                                                          *
 *--------------------------------------------------------------------------*/



int kerfs_add_dir_entry (struct inode *dir,
                         char *filename,
                         container_t * ctnr, umode_t mode, int map_type);

int kerfs_add_dir_entry_no_lock (struct inode *dir,
                                 char *filename,
                                 container_t * ctnr,
                                 umode_t mode, int map_type);

int kerfs_delete_dir_entry (struct inode *dir, struct inode *inode);

int kerfs_delete_dir_entry_no_lock (struct inode *dir, struct inode *inode);

struct kerfs_dir_entry *kerfs_find_grab_dir_entry_by_name (struct inode *dir,
                                                           char *filename);

void kerfs_put_dir_entry (struct kerfs_dir_entry *dir_entry);

struct inode *physical_find_dir_entry_by_name (struct inode *dir,
                                               struct dentry *dentry);

int kerfs_fill_dir (struct inode *dir, struct file *phys_dir);

int kerfs_empty_dir (struct inode *inode);

static inline struct kerfs_dir_entry *next_dir_entry (struct kerfs_dir_entry
                                                      *dir_entry)
{
  unsigned long addr;

  addr = (unsigned long) dir_entry;
  addr += KERFS_DIRENT_SIZE;
  return (struct kerfs_dir_entry *) addr;
}

static inline char *d_path_from_kerfs (struct dentry *dentry,
                                       struct vfsmount *vfsmnt,
                                       char *buf, int buflen)
{
  char *path;
  int len;

  vfsmnt = kerfs_mnt;

  path = __d_path (dentry, vfsmnt, kerfs_root, kerfs_mnt, buf, buflen);
  len = strlen (path);
  if (len >= 10)
    {
      if (strcmp (path + len - 10, " (deleted)") == 0)
        path[len - 10] = 0;
    }

  path = path - strlen (kerfs_phys_root_name);
  memcpy (path, kerfs_phys_root_name, strlen (kerfs_phys_root_name));

  return path;
}

#endif // __KERFS_DIR__
