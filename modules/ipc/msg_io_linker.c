/*
 *  Kerrighed/modules/ipc/msg_io_linker.c
 *
 *  KDDM IPC msg_queue id Linker.
 *
 *  Copyright (C) 2007-2008 Matthieu Fertré - INRIA
 */

#include "debug_keripc.h"

#define MODULE_NAME "IPC Msg"

#include <linux/shm.h>
#include <linux/lockdep.h>
#include <linux/security.h>
#include <kerrighed/ipc.h>

#include <rpc/rpc.h>
#include <ctnr/kddm.h>

#include "msg_io_linker.h"


struct kmem_cache *msq_object_cachep;

/** Create a local instance of a remotly existing IPC message queue.
 *
 *  @author Matthieu Fertré
 */
static struct msg_queue *create_local_msq(struct msg_queue *received_msq)
{
	struct ipc_namespace *ns = &init_ipc_ns; /* TODO: manage namespace */
	struct msg_queue *msq;
	int id;

	DEBUG (DBG_KERIPC_MSG_LINKER, 3, "Create local msgqueue - id %d\n",
	       received_msq->q_id);

	msq = ipc_rcu_alloc(sizeof(*msq));
	if (!msq)
		return ERR_PTR(-ENOMEM);

	*msq = *received_msq;

	id = msq->q_id % SEQ_MULTIPLIER;
	grow_ary (&msg_ids(ns), id + 1);
	msg_ids(ns).in_use++;

	security_msg_queue_alloc(msq);
	mutex_init(&msq->q_perm.mutex);

	msq->is_master = 0;
	INIT_LIST_HEAD(&msq->q_messages);
	INIT_LIST_HEAD(&msq->q_receivers);
	INIT_LIST_HEAD(&msq->q_senders);

	msg_ids(ns).entries->p[id] = &msq->q_perm;

	DEBUG (DBG_KERIPC_MSG_LINKER, 3, "Local msg queue created %d (%p)\n",
	       msq->q_id, msq);

	return msq;
}

/** Remove a local instance of a removed IPC message queue.
 *
 *  @author Matthieu Fertré
 */
static void delete_local_msq(struct ipc_namespace *ns, struct msg_queue *local_msq)
{
	struct msg_queue *msq;

	msq = local_msq;

	security_msg_queue_free(msq);
	ipc_rcu_putref(msq);
}

/** Update a local instance of a remotly existing IPC message queue.
 *
 *  @author Matthieu Fertré
 */
static void update_local_msq (struct msg_queue *local_msq,
			      struct msg_queue *received_msq)
{
	/* local_msq->q_perm = received_msq->q_perm;*/
	local_msq->q_stime = received_msq->q_stime;
	local_msq->q_rtime = received_msq->q_rtime;
	local_msq->q_ctime = received_msq->q_ctime;
	local_msq->q_cbytes = received_msq->q_cbytes;
	local_msq->q_qnum = received_msq->q_qnum;
	local_msq->q_qbytes = received_msq->q_qbytes;
	local_msq->q_lspid = received_msq->q_lspid;
	local_msq->q_lrpid = received_msq->q_lrpid;

	/* Do not modify the list_head else you will loose
	   information on master node */
}

/*****************************************************************************/
/*                                                                           */
/*                         MSQID KDDM IO FUNCTIONS                           */
/*                                                                           */
/*****************************************************************************/



int msq_alloc_object (struct kddm_obj * obj_entry,
		      struct kddm_set * set,
		      objid_t objid)
{
	msq_object_t *msq_object;

	DEBUG (DBG_KERIPC_MSG_LINKER, 3, "Alloc object (%ld;%ld)\n",
	       set->id, objid);

	msq_object = kmem_cache_alloc(msq_object_cachep, GFP_KERNEL);
	if (!msq_object)
		return -ENOMEM;

	msq_object->local_msq = NULL;
	obj_entry->object = msq_object;

	DEBUG (DBG_KERIPC_MSG_LINKER, 3, "Alloc object (%ld;%ld): done %p\n",
	       set->id, objid, msq_object);

	return 0;
}



/** Handle a kddm set msq_queue id first touch
 *  @author Matthieu Fertré
 *
 *  @param  obj_entry  Kddm object descriptor.
 *  @param  set       Kddm set descriptor
 *  @param  objid     Id of the object to create.
 *
 *  @return  0 if everything is ok. Negative value otherwise.
 */
int msq_first_touch (struct kddm_obj * obj_entry,
		     struct kddm_set * set,
		     objid_t objid,
		     int flags)
{
	BUG(); // I should never get here !

	return 0;
}



/** Insert a new msg_queue id in local structures.
 *  @author Matthieu Fertré
 *
 *  @param  obj_entry  Descriptor of the object to insert.
 *  @param  set       Kddm set descriptor
 *  @param  objid     Id of the object to insert.
 */
int msq_insert_object (struct kddm_obj * obj_entry,
		       struct kddm_set * set,
		       objid_t objid)
{
	msq_object_t *msq_object;
	struct msg_queue *msq;

	DEBUG (DBG_KERIPC_MSG_LINKER, 3, "Insert object (%ld;%ld)\n",
	       set->id, objid);

	msq_object = obj_entry->object;
	BUG_ON(!msq_object);

	/* Regular case, the kernel msg_queue struct is already allocated */
	if (msq_object->local_msq) {
		if (msq_object->mobile_msq.q_id != -1)
			update_local_msq(msq_object->local_msq,
					 &msq_object->mobile_msq);
	} else {
		/* This is the first time the object is inserted locally. We need
		 * to allocate kernel msq structures.
		 */
		msq = create_local_msq(&msq_object->mobile_msq);
		msq_object->local_msq = msq;
		BUG_ON(!msq);
	}

	DEBUG (DBG_KERIPC_MSG_LINKER, 3, "Insert object (%ld;%ld) : done %p\n",
	       set->id, objid, msq_object);

	return 0;
}



/** Invalidate a kddm object msqid.
 *  @author Matthieu Fertré
 *
 *  @param  obj_entry  Descriptor of the object to invalidate.
 *  @param  set       Kddm set descriptor
 *  @param  objid     Id of the object to invalidate
 */
int msq_invalidate_object (struct kddm_obj * obj_entry,
			   struct kddm_set * set,
			   objid_t objid)
{
	DEBUG (DBG_KERIPC_MSG_LINKER, 3, "Invalidate object (%ld;%ld)\n",
	       set->id, objid);

	return KDDM_IO_KEEP_OBJECT;
}



/** Handle a msg queue remove.
 *  @author Matthieu Fertré
 *
 *  @param  obj_entry  Descriptor of the object to remove.
 *  @param  set       Kddm set descriptor.
 *  @param  padeid    Id of the object to remove.
 */
int msq_remove_object (void *object,
		       struct kddm_set * set,
		       objid_t objid)
{
	msq_object_t *msq_object;
	struct msg_queue *msq;
	struct ipc_namespace *ns = &init_ipc_ns; /* TODO: manage namespace */

	DEBUG (DBG_KERIPC_MSG_LINKER, 3, "remove object (%ld;%ld)\n",
	       set->id, objid);

	msq_object = object;
	if (msq_object) {
		msq = msq_object->local_msq;
		if (!msq->is_master) {
			DEBUG (DBG_KERIPC_MSG_LINKER, 4, "Removing local msg queue %d (%p)\n",
			       msq->q_id, msq);

			delete_local_msq(ns, msq);
		} else {
			DEBUG (DBG_KERIPC_MSG_LINKER, 4, "Removing MASTER msg queue %d (%p)\n",
			       msq->q_id, msq);
			__freeque(ns, msq, msq->q_id);
		}
		kmem_cache_free (msq_object_cachep, msq_object);
	}

	DEBUG (DBG_KERIPC_MSG_LINKER, 3, "remove object (%ld;%ld) : done\n",
	       set->id, objid);

	return 0;
}



/** Export an object
 *  @author Matthieu Fertré
 *
 *  @param  buffer    Buffer to export object data in.
 *  @param  object    The object to export data from.
 */
int msq_export_object (struct rpc_desc *desc,
			 struct kddm_obj *obj_entry)
{
	msq_object_t *msq_object;
	DEBUG (DBG_KERIPC_MSG_LINKER, 3, "Import object %p\n",
	       obj_entry->object);
	msq_object = obj_entry->object;
	msq_object->mobile_msq = *msq_object->local_msq;

	rpc_pack(desc, 0, msq_object, sizeof(msq_object_t));

	DEBUG (DBG_KERIPC_MSG_LINKER, 3, "Import object %p : done\n",
	       obj_entry->object);
	return 0;
}



/** Import an object
 *  @author Matthieu Fertré
 *
 *  @param  object    The object to import data in.
 *  @param  buffer    Data to import in the object.
 */
int msq_import_object (struct kddm_obj *obj_entry,
		       struct rpc_desc *desc)
{
	msq_object_t *msq_object, buffer;
	struct msg_queue *msq;

	msq_object = obj_entry->object;
	DEBUG (DBG_KERIPC_MSG_LINKER, 3, "Import object %p\n",
	       msq_object);

	rpc_unpack(desc, 0, &buffer, sizeof(msq_object_t));

	msq_object->mobile_msq = buffer.mobile_msq;

	if (msq_object->local_msq) {
		msq = msq_object->local_msq;
	}
	DEBUG (DBG_KERIPC_MSG_LINKER, 3, "Import object %p : done\n",
	       msq_object);
	return 0;
}



/****************************************************************************/

/* Init the msg queue id IO linker */

struct iolinker_struct msq_linker = {
	first_touch:       msq_first_touch,
	remove_object:     msq_remove_object,
	invalidate_object: msq_invalidate_object,
	insert_object:     msq_insert_object,
	linker_name:       "msg_queue",
	linker_id:         MSG_LINKER,
	alloc_object:      msq_alloc_object,
	export_object:     msq_export_object,
	import_object:     msq_import_object
};



/*****************************************************************************/
/*                                                                           */
/*                  MSG QUEUE KEY KDDM IO FUNCTIONS                          */
/*                                                                           */
/*****************************************************************************/

/* Init the msg queue key IO linker */

struct iolinker_struct msqkey_linker = {
	linker_name:       "msqkey",
	linker_id:         MSGKEY_LINKER,
};

/*****************************************************************************/
/*                                                                           */
/*                  MSG MASTER KDDM IO FUNCTIONS                             */
/*                                                                           */
/*****************************************************************************/

/* Init the msg master node IO linker */

struct iolinker_struct msqmaster_linker = {
	linker_name:       "msqmaster",
	linker_id:         MSGMASTER_LINKER,
};
