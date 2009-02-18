/** File ghost interface
 *  @file file_ghost.h
 *
 *  Definition of file ghost structures and functions.
 *  @author Renaud Lottiaux
 */

#ifndef __FILE_GHOST__
#define __FILE_GHOST__


#define MAX_LENGHT_STRING 128



/*--------------------------------------------------------------------------*
 *                                                                          *
 *                                  TYPES                                   *
 *                                                                          *
 *--------------------------------------------------------------------------*/



/** File ghost private data
 */
typedef struct file_ghost_data {
  struct file *file;              /**< File to save/load data to/from */
  int from_fd;
} file_ghost_data_t ;



/*--------------------------------------------------------------------------*
 *                                                                          *
 *                              EXTERN FUNCTIONS                            *
 *                                                                          *
 *--------------------------------------------------------------------------*/

int mkdir_chkpt_path(long app_id, unsigned int chkpt_sn);

char * get_chkpt_dir(long app_id, unsigned int chkpt_sn);

char * get_chkpt_filebase(long app_id,
			  unsigned int chkpt_sn,
			  int obj_id,
			  const char * obj_prefix);

/** Create a new file ghost.
 *  @author Matthieu Fertré, Renaud Lottiaux
 *
 *  @param  access   Ghost access (READ/WRITE)
 *  @param  file     File to read/write data to/from.
 *
 *  @return        ghost_t if everything ok
 *                 ERR_PTR otherwise.
 */
ghost_t *create_file_ghost(int access,
			   long app_id,
			   unsigned int chkpt_sn,
			   int obj_id,
			   const char * label);

/** Create a new file ghost.
 *  @author Matthieu Fertré, Renaud Lottiaux
 *
 *  @param  access   Ghost access (READ/WRITE)
 *  @param  fd       File descriptor to read/write data to/from.
 *
 *  @return        ghost_t if everything ok
 *                 ERR_PTR otherwise.
 */
ghost_t *create_file_ghost_from_fd(int access, unsigned int fd);

typedef struct {
	mm_segment_t fs;
	uid_t uid;
	gid_t gid;
} ghost_fs_t;

ghost_fs_t set_ghost_fs(uid_t uid, gid_t gid);
void unset_ghost_fs(ghost_fs_t * oldfs);

#endif // __FILE_GHOST__
