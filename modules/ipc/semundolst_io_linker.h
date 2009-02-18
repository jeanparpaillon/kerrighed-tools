/** Container SEM_UNDO proc_list Linker.
 *  @file semundolst_io_linker.h
 *
 *  Link containers and Linux SEM_UNDO proc list mechanisms.
 *  @author Matthieu Fertré
 */

#ifndef __SEMUNDOLST_IO_LINKER__
#define __SEMUNDOLST_IO_LINKER__

#include <ctnr/kddm_types.h>

/*--------------------------------------------------------------------------*
 *                                                                          *
 *                                  TYPES                                   *
 *                                                                          *
 *--------------------------------------------------------------------------*/

struct semundo_id {
	int semid;
	struct semundo_id * next;
};

typedef struct semundo_list_object {
	unique_id_t id;
	atomic_t refcnt;
	atomic_t semcnt;
	struct semundo_id * list;
} semundo_list_object_t;

extern struct iolinker_struct semundo_linker;

#endif
