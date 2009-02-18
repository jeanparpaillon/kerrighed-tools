/** Global management of regular files interface.
 *  @file regular_file_mgr.h
 *
 *  @author Renaud Lottiaux
 */


#ifndef __REGULAR_FILE_MGR__
#define __REGULAR_FILE_MGR__


#include <ctnr/kddm_types.h>
#include <ghost/ghost.h>


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



typedef struct regular_file_krg_desc {
	kddm_set_id_t ctnrid;
	unique_id_t file_objid;
	unsigned int flags;
	mode_t mode;
	loff_t pos;
	int shmid;
	int sysv;
	unsigned int uid;
	unsigned int gid;
	char filename[1];
} regular_file_krg_desc_t;



/*--------------------------------------------------------------------------*
 *                                                                          *
 *                            EXTERN VARIABLES                              *
 *                                                                          *
 *--------------------------------------------------------------------------*/



extern struct dvfs_mobility_operations dvfs_mobility_regular_ops;



/*--------------------------------------------------------------------------*
 *                                                                          *
 *                              EXTERN FUNCTIONS                            *
 *                                                                          *
 *--------------------------------------------------------------------------*/

int ghost_read_file_krg_desc(ghost_t *ghost, void **desc);
int ghost_write_file_krg_desc(ghost_t *ghost, void *desc, int desc_size);

struct file *begin_import_dvfs_file(unsigned long dvfs_objid,
				    struct dvfs_file_struct **dvfs_file);

int end_import_dvfs_file(unsigned long dvfs_objid,
			 struct dvfs_file_struct *dvfs_file,
			 struct file *file, int first_import);

struct file *import_regular_file_from_krg_desc (struct epm_action * action,
						struct task_struct *task,
                                                void *_desc);


#endif // __REGULAR_FILE_MGR__
