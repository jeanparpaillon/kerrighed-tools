/** DVFS Level 3 - File struct sharing management.
 *  @file file.h
 *  
 *  @author Renaud Lottiaux 
 */

#ifndef __DVFS_FILE__
#define __DVFS_FILE__

#include <ghost/ghost_types.h>
#include "file_struct_io_linker.h"
#include <ctnr/kddm.h>

struct epm_action;

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


extern struct dvfs_file_struct *dvfs_file;
extern struct kddm_set *dvfs_file_struct_ctnr;


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



int create_ctnr_file_object(struct file *file);

void check_file_struct_sharing (int index, struct file *file,
				struct epm_action *action);

void get_dvfs_file(int index, unsigned long objid);
void put_dvfs_file(int index, struct file *file);

int dvfs_file_init(void);
void dvfs_file_finalize(void);

static inline struct dvfs_file_struct *grab_dvfs_file_struct(unsigned long file_id)
{
	struct dvfs_file_struct * dvfs_file;

	dvfs_file = _kddm_grab_object(dvfs_file_struct_ctnr, file_id);
	if (dvfs_file && dvfs_file->file) {
		if (atomic_read (&dvfs_file->file->f_count) == 0)
			dvfs_file->file = NULL;
	}
	return dvfs_file;
}

static inline struct dvfs_file_struct *get_dvfs_file_struct(unsigned long file_id)
{
	struct dvfs_file_struct * dvfs_file;
	
	dvfs_file = _kddm_get_object(dvfs_file_struct_ctnr, file_id);
	if (dvfs_file && dvfs_file->file) {
		if (atomic_read (&dvfs_file->file->f_count) == 0)
			dvfs_file->file = NULL;
	}
	return dvfs_file;
}

static inline void put_dvfs_file_struct(unsigned long file_id)
{
	_kddm_put_object (dvfs_file_struct_ctnr, file_id);
}

#endif // __KERFS_FILE__
