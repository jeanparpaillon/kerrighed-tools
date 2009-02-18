/** KDDM IO linker interface.
 *  @file io_linker.c
 *  
 *  Copyright (C) 2001-2006, INRIA, Universite de Rennes 1, EDF.
 *  Copyright (C) 2006-2007, Renaud Lottiaux, Kerlabs.
 */

#include <linux/slab.h>
#include <linux/mman.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <kerrighed/sys/types.h>
#include <kerrighed/krginit.h>
#include <kerrighed/krgflags.h>

#include <rpc/rpc.h>
#include "kddm.h"
#include "io_linker.h"
#include <tools/debug.h>
#include "debug_kddm.h"



struct iolinker_struct *iolinker_list[MAX_IO_LINKER];



/** return the default owner of an object named with an unique id.
 *  @author Renaud Lottiaux
 *
 *  @param obj_entry  Entry of the object to set the state.
 *  @param state      State to set the object with.
 */
kerrighed_node_t unique_id_default_owner (struct kddm_set * set, objid_t objid,
					  const krgnodemask_t *nodes,
					  int nr_nodes)
{
	kerrighed_node_t nodeid;

	nodeid = objid >> UNIQUE_ID_NODE_SHIFT;

	return nodeid;
}



/*****************************************************************************/
/*                                                                           */
/*                     INSTANTIATE/UNINSTANTIATE FUNCTIONS                   */
/*                                                                           */
/*****************************************************************************/



/** Instantiate a kddm set with an IO linker.
 *  @author Renaud Lottiaux
 *
 *  @param set           Kddm set to instantiate.
 *  @param link          Node linked to the kddm set.
 *  @param iolinker_id   Id of the linker to link to the kddm set.
 *  @param private_data  Data used by the instantiator...
 *
 *  @return  Structure of the requested kddm set or NULL if not found.
 */
int kddm_io_instantiate (struct kddm_set * set,
			 kerrighed_node_t def_owner,
			 iolinker_id_t iolinker_id,
			 void *private_data,
			 int data_size, 
			 int master)
{
	int err = 0;
	
	BUG_ON (set == NULL);
	BUG_ON (iolinker_id < 0 || iolinker_id >= MAX_IO_LINKER);
	BUG_ON (set->state != KDDM_SET_LOCKED);
	
	while (iolinker_list[iolinker_id] == NULL) {
		WARNING ("Instantiate a kddm set with a not registered IO "
			 "linker (%d)... Retry in 1 second\n", iolinker_id);
		set_current_state (TASK_INTERRUPTIBLE);
		schedule_timeout (1 * HZ);
	}
	
	set->def_owner = def_owner;
	set->iolinker = iolinker_list[iolinker_id];
	
	if (data_size) {
		set->private_data = kmalloc (data_size, GFP_KERNEL);
		BUG_ON (set->private_data == NULL);
		memcpy (set->private_data, private_data, data_size);
		set->private_data_size = data_size;
	}
	else {
		set->private_data = NULL;
		set->private_data_size = 0;
	}

	if (set->iolinker->instantiate)
		err = set->iolinker->instantiate (set, private_data,
						  master);

	return err;
}



/** Uninstantiate a kddm set.
 *  @author Renaud Lottiaux
 *
 *  @param set          Kddm Set to uninstantiate
 */
void kddm_io_uninstantiate (struct kddm_set * set,
                            int destroy)
{
	if (set->iolinker && set->iolinker->uninstantiate)
		set->iolinker->uninstantiate (set, destroy);
	
	if (set->private_data)
		kfree(set->private_data);
	set->private_data = NULL;
	set->iolinker = NULL;
}



/*****************************************************************************/
/*                                                                           */
/*                      MAIN IO LINKER INTERFACE FUNCTIONS                   */
/*                                                                           */
/*****************************************************************************/



/** Request an IO linker to allocate an object.
 *  @author Renaud Lottiaux
 *
 *  @param obj_entry    Object entry to export data from.
 *  @param set          Kddm Set the object belong to.
 */
int kddm_io_alloc_object (struct kddm_obj * obj_entry,
			  struct kddm_set * set,
			  objid_t objid)
{
	int r = 0;

	if (obj_entry->object != NULL)
		goto done;

	if (set->iolinker && set->iolinker->alloc_object)
		r = set->iolinker->alloc_object (obj_entry, set, objid);
	else {
		/* Default allocation function */
		obj_entry->object = kmalloc(set->obj_size, GFP_KERNEL);
		if (obj_entry->object == NULL)
			r = -ENOMEM;
	}

	if (obj_entry->object != NULL)
		atomic_inc(&set->nr_objects);

done:
	return r;
}



/** Request an IO linker to do an object first touch.
 *  @author Renaud Lottiaux
 *
 *  @param set          Kddm Set the object belong to.
 *  @param objid        Id of the object to first touch.
 *  @param obj_entry    Object entry the object belong to.
 */
int kddm_io_first_touch_object (struct kddm_obj * obj_entry,
                                struct kddm_set * set,
                                objid_t objid,
				int flags)
{
	int res = 0 ;
	
	BUG_ON (obj_entry->object != NULL);
	BUG_ON (OBJ_STATE(obj_entry) != INV_FILLING);
	
	if (set->iolinker && set->iolinker->first_touch) {
		res = set->iolinker->first_touch (obj_entry, set,
						  objid, flags);
		if (obj_entry->object)
			atomic_inc(&set->nr_objects);
	}
	else
		res = kddm_io_alloc_object(obj_entry, set, objid);
	
	return res ;
}



/** Request an IO linker to insert an object in a kddm set.
 *  @author Renaud Lottiaux
 *
 *  @param set          Kddm Set the object belong to.
 *  @param objid        Id of the object to insert.
 *  @param obj_entry    Object entry the object belong to.
 */
int kddm_io_insert_object (struct kddm_obj * obj_entry,
                           struct kddm_set * set,
                           objid_t objid)
{
	int res = 0;
	
	if (set->iolinker && set->iolinker->insert_object)
		res = set->iolinker->insert_object (obj_entry, set,
						    objid);

	return res;
}



/** Request an IO linker to put a kddm object.
 *  @author Renaud Lottiaux
 *
 *  @param set          Kddm Set the object belong to.
 *  @param objid        Id of the object to put.
 *  @param obj_entry    Object entry the object belong to.
 */
int kddm_io_put_object (struct kddm_obj * obj_entry,
                        struct kddm_set * set,
                        objid_t objid)
{
	int res = 0;
	
	ASSERT_OBJ_LOCKED(set, objid);
	
	if (set && set->iolinker->put_object)
		res = set->iolinker->put_object (obj_entry, set,
						 objid);
	
	return res;
}



/** Request an IO linker to invalidate an object.
 *  @author Renaud Lottiaux
 *
 *  @param set          Kddm Set the object belong to.
 *  @param objid        Id of the object to invalidate.
 *  @param obj_entry    Object entry the object belong to.
 */
int kddm_io_invalidate_object (struct kddm_obj * obj_entry,
			       struct kddm_set * set, 
			       objid_t objid)
{
	int res = 0;
	
	ASSERT_OBJ_LOCKED(set, objid);

	if (obj_entry->object) {
		if (set->iolinker && set->iolinker->invalidate_object) {
			res = set->iolinker->invalidate_object (obj_entry,
								set, objid);
			
			if (res != KDDM_IO_KEEP_OBJECT)
				obj_entry->object = NULL;
		}
		
		if (obj_entry->object == NULL)
			atomic_dec(&set->nr_objects);
	}

	return res;
}



/** Request an IO linker to remove an object.
 *  @author Renaud Lottiaux
 *
 *  @param set          Kddm Set the object belong to.
 *  @param objid        Id of the object to remove.
 *  @param obj_entry    Object entry the object belong to.
 */
int kddm_io_remove_object (void *object,
			   struct kddm_set * set,
			   objid_t objid)
{
	int res = 0;

	if (set->iolinker && set->iolinker->remove_object) {
		might_sleep();
		res = set->iolinker->remove_object (object, set, objid);
	}
	else
		/* Default free function */
		kfree (object);

	atomic_dec(&set->nr_objects);

	return res;
}

int kddm_io_remove_object_and_unlock (struct kddm_obj * obj_entry,
				      struct kddm_set * set,
				      objid_t objid)
{
	int res = 0;
	void *object;
	
	ASSERT_OBJ_LOCKED(set, objid);

	object = obj_entry->object;
	
	if (object == NULL) {
		kddm_obj_unlock(set, objid);
		goto done;
	}

	obj_entry->object = NULL;
	kddm_obj_unlock(set, objid);

	res = kddm_io_remove_object (object, set, objid);

done:
	return res;
}



/** Request an IO linker to sync an object.
 *  @author Renaud Lottiaux
 *
 *  @param set          Kddm Set the object belong to.
 *  @param objid        Id of the object to sync.
 *  @param obj_entry    Object entry the object belong to.
 */
int kddm_io_sync_object (struct kddm_obj * obj_entry,
                         struct kddm_set * set,
                         objid_t objid)
{
	int res = 0 ;
	
	if (set->iolinker && set->iolinker->sync_object)
		res = set->iolinker->sync_object (obj_entry, set, objid);
	else
		BUG();

	return res ;
}



/** Inform an IO linker that an object state has changed.
 *  @author Renaud Lottiaux
 *
 *  @param obj_entry    Object entry the object belong to.
 *  @param set          Kddm Set the object belong to.
 *  @param objid        Id of the object to sync.
 *  @param new_state    New state for the object.
 */
int kddm_io_change_state (struct kddm_obj * obj_entry,
			  struct kddm_set * set,
			  objid_t objid, 
			  kddm_obj_state_t new_state)
{
	if (set->iolinker && set->iolinker->change_state)
		set->iolinker->change_state (obj_entry, set, objid, new_state);
	
	return 0 ;
}



/** Request an IO linker to import data into an object.
 *  @author Renaud Lottiaux
 *
 *  @param set          Kddm Set the object belong to.
 *  @param obj_entry    Object entry to import data into.
 *  @param buffer       Buffer containing data to import.
 */
int kddm_io_import_object (struct kddm_set *set,
                           struct kddm_obj *obj_entry,
                           struct rpc_desc *desc)
{
	struct iolinker_struct *io = set->iolinker;
	int res;

	BUG_ON (OBJ_STATE(obj_entry) != INV_FILLING);

	might_sleep();

	if (io && io->import_object)
		res = io->import_object(obj_entry, desc);
	else
		res = rpc_unpack(desc, 0, obj_entry->object, set->obj_size);

	return res;
}



/** Request an IO linker to export data from an object.
 *  @author Renaud Lottiaux
 *
 *  @param set          Kddm Set the object belong to.
 *  @param obj_entry    Object entry to export data from.
 *  @param desc		RPC descriptor to export data on.
 */
int kddm_io_export_object (struct kddm_set * set,
			   struct rpc_desc *desc,
                           struct kddm_obj *obj_entry)
{
	struct iolinker_struct *io = set->iolinker;
	int res;

	if (io && io->export_object)
		res = io->export_object(desc, obj_entry);
	else
		res = rpc_pack(desc, 0, obj_entry->object, set->obj_size);

	return res;
}



kerrighed_node_t __kddm_io_default_owner (struct kddm_set *set,
					  objid_t objid,
					  const krgnodemask_t *nodes,
					  int nr_nodes)
{
	switch (set->def_owner) {
	  case KDDM_RR_DEF_OWNER:
		  if (likely(__krgnode_isset(kerrighed_node_id, nodes)))
			  return __nth_krgnode(objid % nr_nodes, nodes);
		  else
			  return kerrighed_node_id;

	  case KDDM_CUSTOM_DEF_OWNER:
		  return set->iolinker->default_owner (set, objid,
						       nodes, nr_nodes);

	  default:
		  return set->def_owner;
	}
}



/** Freeze an object.
 *  @author Renaud Lottiaux
 *
 *  @param obj_entry     Object entry to freeze.
 */
void kddm_io_freeze_object (struct kddm_obj * obj_entry,
                            struct kddm_set * set)
{
	if (set->iolinker && set->iolinker->freeze_object)
		return set->iolinker->freeze_object (obj_entry);
}



/** Warm an object.
 *  @author Renaud Lottiaux
 *
 *  @param obj_entry     Object entry to warm.
 */
void kddm_io_warm_object (struct kddm_obj * obj_entry,
                          struct kddm_set * set)
{
	if (set->iolinker && set->iolinker->warm_object)
		return set->iolinker->warm_object (obj_entry);
}



/** Test if an object is frozen.
 *  @author Renaud Lottiaux
 *
 *  @param obj_entry     Object entry to warm.
 */
int kddm_io_is_frozen (struct kddm_obj * obj_entry,
                       struct kddm_set * set)
{
	if (set->iolinker && set->iolinker->is_frozen && obj_entry->object)
		return set->iolinker->is_frozen (obj_entry);
	else
		return 0;
}



/*****************************************************************************/
/*                                                                           */
/*                           IO LINKER INIT FUNCTIONS                        */
/*                                                                           */
/*****************************************************************************/



/** Register a new kddm set IO linker.
 *  @author Renaud Lottiaux
 *
 *  @param io_linker_id
 *  @param linker
 */
int register_io_linker (int linker_id,
                        struct iolinker_struct *io_linker)
{
	if(iolinker_list[linker_id] != NULL)
		return -1;

	iolinker_list[linker_id] = io_linker;

	return 0;
}



/** Initialise the IO linker array with existing linker
 */
void io_linker_init (void)
{
	int i;
	
	for (i = 0; i < MAX_IO_LINKER; i++)
		iolinker_list[i] = NULL;
}



/** Initialise the IO linker array with existing linker
 */
void io_linker_finalize (void)
{
}
