/*
 *  Kerrighed/modules/ipc/semundolst_io_linker.c
 *
 *  KDDM SEM undo proc list Linker.
 *
 *  Copyright (C) 2007-2008 Matthieu Fertré - INRIA
 */

#include "debug_keripc.h"

#define MODULE_NAME "Sem undo list linker"

#include <linux/sem.h>
#include <linux/lockdep.h>
#include <linux/security.h>
#include <kerrighed/ipc.h>

#include <rpc/rpc.h>
#include <ctnr/kddm.h>

#include "semundolst_io_linker.h"


/*****************************************************************************/
/*                                                                           */
/*                         SEM Undo list KDDM IO FUNCTIONS                   */
/*                                                                           */
/*****************************************************************************/

static inline void __undolist_remove(semundo_list_object_t *undo_list)
{
	struct semundo_id *id, *next;

	if (undo_list) {

		DEBUG(DBG_KERIPC_SEMUNDO_LINKER, 5,
		      "Process %d has %d sem_undos\n",
		      current->pid, atomic_read(&undo_list->semcnt));

		for (id = undo_list->list; id; id = next) {
			next = id->next;
			DEBUG(DBG_KERIPC_SEMUNDO_LINKER, 5,
			      "kfree undo_id %p (%d)\n", id, id->semid);
			kfree(id);
		}
		undo_list->list = NULL;
	}
}

static inline semundo_list_object_t * __undolist_alloc(void)
{
	semundo_list_object_t *undo_list;

	undo_list = kzalloc(sizeof(semundo_list_object_t), GFP_KERNEL);
	if (!undo_list)
		return ERR_PTR(-ENOMEM);

	return undo_list;
}

/** Handle a kddm set sem_undo_list alloc
 *  @author Matthieu Fertré
 *
 *  @param  obj_entry  Kddm object descriptor.
 *  @param  set       Kddm set descriptor
 *  @param  objid     Id of the object to create.
 */
int undolist_alloc_object (struct kddm_obj * obj_entry,
			   struct kddm_set * set,
			   objid_t objid)
{
	semundo_list_object_t *undo_list;

	DEBUG (DBG_KERIPC_SEMUNDO_LINKER, 1,
	       "Alloc object (%ld;%ld), obj_entry %p\n",
	       set->id, objid, obj_entry);

	undo_list = __undolist_alloc();

	obj_entry->object = undo_list;

	DEBUG (DBG_KERIPC_SEMUNDO_LINKER, 1,
	       "Alloc object (%ld;%ld): done %p\n",
	       set->id, objid, undo_list);

	return 0;
}


/** Handle a kddm set sem_undo_list first touch
 *  @author Matthieu Fertré
 *
 *  @param  obj_entry  Kddm object descriptor.
 *  @param  set       Kddm set descriptor
 *  @param  objid     Id of the object to create.
 *
 *  @return  0 if everything is ok. Negative value otherwise.
 */
int undolist_first_touch (struct kddm_obj * obj_entry,
			  struct kddm_set * set,
			  objid_t objid,
			  int flags)
{
	DEBUG (DBG_KERIPC_SEMUNDO_LINKER, 1, "first touch: %ld, %ld\n",
	       set->id, objid);
	BUG();

	return 0;
}


/** Handle a kddm sem_undo_list remove.
 *  @author Matthieu Fertré
 *
 *  @param  obj_entry  Descriptor of the object to remove.
 *  @param  set       Kddm set descriptor.
 *  @param  padeid    Id of the object to remove.
 */
int undolist_remove_object (void *object,
			    struct kddm_set * set,
			    objid_t objid)
{
	semundo_list_object_t *undo_list;

	DEBUG (DBG_KERIPC_SEMUNDO_LINKER, 1, "remove object (%ld;%ld)\n",
	       set->id, objid);

	undo_list = object;

	__undolist_remove(undo_list);
	kfree(undo_list);
	object = NULL;

	DEBUG (DBG_KERIPC_SEMUNDO_LINKER, 1,
	       "remove object (%ld;%ld) : done\n",
	       set->id, objid);

	return 0;
}

/** Invalidate a kddm sem_undo_list
 *  @author Matthieu Fertré
 *
 *  @param  obj_entry  Descriptor of the object to invalidate.
 *  @param  set       Kddm set descriptor
 *  @param  objid     Id of the object to invalidate
 */
int undolist_invalidate_object (struct kddm_obj * obj_entry,
				struct kddm_set * set,
				objid_t objid)
{
	semundo_list_object_t *undo_list;
	DEBUG (DBG_KERIPC_SEMUNDO_LINKER, 1, "Invalidate object (%ld;%ld)\n",
	       set->id, objid);

	undo_list = obj_entry->object;
	__undolist_remove(undo_list);
	obj_entry->object = NULL;

	return 0;
}


/** Export a sem_undo_list
 *  @author Matthieu Fertré
 *
 *  @param  buffer    Buffer to export object data in.
 *  @param  object    The object to export data from.
 */
int undolist_export_object (struct rpc_desc *desc,
			    struct kddm_obj *obj_entry)
{
	semundo_list_object_t *undo_list;
	struct semundo_id *un;
	int nb_semundo = 0;

	undo_list = obj_entry->object;

	DEBUG (DBG_KERIPC_SEMUNDO_LINKER, 1, "export undolist\n");

	rpc_pack_type(desc, *undo_list);

	/* counting number of semundo to send */
	for (un = undo_list->list; un;  un = un->next)
		nb_semundo++;

	rpc_pack_type(desc, nb_semundo);

	BUG_ON(nb_semundo != atomic_read(&undo_list->semcnt));

	/* really sending the semundo identifier */
	for (un = undo_list->list; un;  un = un->next)
		rpc_pack_type(desc, *un);

	DEBUG (DBG_KERIPC_SEMARRAY_LINKER, 1, "export undolist: done\n");

	return 0;
}


/** Import a sem_undo_list
 *  @author Matthieu Fertré
 *
 *  @param  object    The object to import data in.
 *  @param  buffer    Data to import in the object.
 */
int undolist_import_object (struct kddm_obj *obj_entry,
			    struct rpc_desc *desc)
{
	semundo_list_object_t *undo_list;
	struct semundo_id *un, *prev = NULL;
	int nb_semundo = 0, i=0;

	undo_list = obj_entry->object;

	rpc_unpack_type(desc, *undo_list);

	DEBUG (DBG_KERIPC_SEMUNDO_LINKER, 1, "import undolist\n");

	rpc_unpack_type(desc, nb_semundo);

	BUG_ON(nb_semundo != atomic_read(&undo_list->semcnt));

	for (i=0; i < nb_semundo; i++) {
		un = kmalloc(sizeof(struct semundo_id), GFP_KERNEL);
		if (!un)
			return -ENOMEM;

		rpc_unpack_type(desc, *un);
		un->next = NULL;
		if (prev)
			prev->next = un;
		else
			undo_list->list = un;
		prev = un;
	}

	DEBUG (DBG_KERIPC_SEMARRAY_LINKER, 1,
	       "import undolist: done\n");

	return 0;
}



/****************************************************************************/

/* Init the sem_undo_list IO linker */

struct iolinker_struct semundo_linker = {
	first_touch:       undolist_first_touch,
	remove_object:     undolist_remove_object,
	invalidate_object: undolist_invalidate_object,
	linker_name:       "semundo",
	linker_id:         SEMUNDO_LINKER,
	alloc_object:      undolist_alloc_object,
	export_object:     undolist_export_object,
	import_object:     undolist_import_object
};
