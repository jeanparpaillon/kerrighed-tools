/*
 *  Kerrighed/modules/proc/sighand_management.c
 *
 *  Copyright (C) 1999-2006 INRIA, Universite de Rennes 1, EDF
 *  Copyright (C) 2006-2007 Pascal Gallard - Kerlabs, Louis Rilling - Kerlabs
 */

#include <linux/types.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/rwsem.h>
#include <kerrighed/signal.h>
#include <kerrighed/pid.h>
#include <kerrighed/task.h>

#include "debug_proc.h"

#define MODULE_NAME "sighand"

#include <rpc/rpc.h>
#include <hotplug/hotplug.h>
#include <ctnr/kddm.h>
#include <proc/task.h>

struct sighand_struct_kddm_object {
	struct sighand_struct *sighand;
	atomic_t count;
	int keep_on_remove;
	struct rw_semaphore remove_sem;
};

static struct kmem_cache *sighand_struct_kddm_obj_cachep;

/* Kddm set of 'struct sighand_struct' location */
static struct kddm_set *sighand_struct_kddm_set = NULL;

/* unique_id for sighand kddm objects */
static unique_id_root_t sighand_struct_id_root;

/*
 * @author Pascal Gallard
 */
static int sighand_struct_alloc_object(struct kddm_obj *obj_entry,
				       struct kddm_set *set, objid_t objid)
{
	struct sighand_struct_kddm_object *obj;
	struct sighand_struct *sig;

	obj = kmem_cache_alloc(sighand_struct_kddm_obj_cachep, GFP_KERNEL);
	DEBUG(DBG_SIGHAND, 3, "%lu (0x%p)\n", objid, obj);
	if (!obj)
		return -ENOMEM;

	sig = kmem_cache_alloc(sighand_cachep, GFP_KERNEL);
	if (!sig) {
		kmem_cache_free(sighand_struct_kddm_obj_cachep, obj);
		return -ENOMEM;
	}

	spin_lock_init(&sig->siglock);
	sig->krg_objid = objid;
	sig->kddm_obj = obj;

	obj->sighand = sig;

	obj->keep_on_remove = 0;
	init_rwsem(&obj->remove_sem);
	obj_entry->object = obj;

	return 0;
}

/*
 * @author Pascal Gallard
 */
static int sighand_struct_first_touch(struct kddm_obj *obj_entry,
				      struct kddm_set *set, objid_t objid,
				      int flags)
{
	struct sighand_struct_kddm_object *obj;
	int r;

	r = sighand_struct_alloc_object(obj_entry, set, objid);
	DEBUG(DBG_SIGHAND, 3, "%lu (0x%p)\n", objid, obj_entry->object);
	if (r)
		return r;
	obj = obj_entry->object;
	atomic_set(&obj->count, 1);

	return 0;
}

/*
 * @author Pascal Gallard
 */
static int sighand_struct_import_object(struct kddm_obj *obj_entry,
					struct rpc_desc *desc)
{
	struct sighand_struct_kddm_object *obj = obj_entry->object;
	struct sighand_struct *dest;
	struct sighand_struct tmp;
	int retval;

	DEBUG(DBG_SIGHAND, 3, "%lu (0x%p)\n", obj->sighand->krg_objid, obj);

	retval = rpc_unpack_type(desc, tmp);
	if (likely(!retval))
		retval = rpc_unpack_type(desc, obj->count);

	if (likely(!retval)) {
		dest = obj->sighand;
		spin_lock_irq(&dest->siglock);
		/* This is safe since all changes are protected by grab, and
		 * no thread can hold a grab during import */
		atomic_set(&dest->count, atomic_read(&tmp.count));
		memcpy(dest->action, tmp.action, sizeof(dest->action));
		memcpy(dest->krg_action, tmp.krg_action,
		       sizeof(dest->krg_action));
		spin_unlock_irq(&dest->siglock);
	}

	return retval;
}

/*
 * @author Pascal Gallard
 */
static int sighand_struct_export_object(struct rpc_desc *desc,
					struct kddm_obj *obj_entry)
{
	struct sighand_struct_kddm_object *obj = obj_entry->object;
	struct sighand_struct *src;
	int retval;

	DEBUG(DBG_SIGHAND, 3, "%lu (0x%p)\n", obj->sighand->krg_objid, obj);

	src = obj->sighand;
	retval = rpc_pack_type(desc, *src);
	if (likely(!retval))
		retval = rpc_pack_type(desc, obj->count);

	return retval;
}

void kcb_sighand_struct_pin(struct sighand_struct *sig)
{
	struct sighand_struct_kddm_object *obj = sig->kddm_obj;
	BUG_ON(!obj);
	down_read(&obj->remove_sem);
}

void kcb_sighand_struct_unpin(struct sighand_struct *sig)
{
	struct sighand_struct_kddm_object *obj = sig->kddm_obj;
	BUG_ON(!obj);
	up_read(&obj->remove_sem);
}

static int sighand_struct_remove_object(void *object,
					struct kddm_set *set, objid_t objid)
{
	struct sighand_struct_kddm_object *obj = object;

	/* Ensure that no thread uses this sighand_struct copy */
	down_write(&obj->remove_sem);
	up_write(&obj->remove_sem);

	DEBUG(DBG_SIGHAND, 3, "%lu (0x%p), keep=%d, %d\n",
	      objid, obj, obj->keep_on_remove,
	      atomic_read(&obj->sighand->count));

	if (!obj->keep_on_remove)
		kmem_cache_free(sighand_cachep, obj->sighand);
	kmem_cache_free(sighand_struct_kddm_obj_cachep, obj);

	return 0;
}

static struct iolinker_struct sighand_struct_io_linker = {
	.first_touch   = sighand_struct_first_touch,
	.linker_name   = "sigh ",
	.linker_id     = SIGHAND_STRUCT_LINKER,
	.alloc_object  = sighand_struct_alloc_object,
	.export_object = sighand_struct_export_object,
	.import_object = sighand_struct_import_object,
	.remove_object = sighand_struct_remove_object,
	.default_owner = unique_id_default_owner,
};

/*
 * Get and lock a sighand structure for a given process
 * @author Pascal Gallard
 */
struct sighand_struct *kcb_sighand_struct_readlock(objid_t id)
{
	struct sighand_struct_kddm_object *obj;

	if (id == 0)
		return NULL;
	obj = _kddm_get_object_no_ft(sighand_struct_kddm_set, id);
	DEBUG(DBG_SIGHAND, 2, "%lu (0x%p)\n", id, obj);
	if (!obj)
		return NULL;

	return obj->sighand;
}

static struct sighand_struct_kddm_object *sighand_struct_writelock(objid_t id)
{
	struct sighand_struct_kddm_object *obj;

	if (id == 0)
		return NULL;
	obj = _kddm_grab_object_no_ft(sighand_struct_kddm_set, id);
	DEBUG(DBG_SIGHAND, 2, "%lu (0x%p)\n", id, obj);
	return obj;
}

/*
 * Grab and lock a sighand structure for a given process
 * @author Pascal Gallard
 */
struct sighand_struct *kcb_sighand_struct_writelock(objid_t id)
{
	struct sighand_struct_kddm_object *obj;

	obj = sighand_struct_writelock(id);
	if (!obj)
		return NULL;

	return obj->sighand;
}

/*
 * unlock a sighand structure for a given process
 * @author Pascal Gallard
 */
void kcb_sighand_struct_unlock(objid_t id)
{
	if (id == 0)
		return;
	DEBUG(DBG_SIGHAND, 2, "%lu\n", id);
	_kddm_put_object(sighand_struct_kddm_set, id);
}

/*
 * Alloc a dedicated sighand_struct to task_struct task.
 * @author Pascal Gallard
 */
struct sighand_struct *kcb_malloc_sighand_struct(struct task_struct *task,
						 int need_update)
{
	struct sighand_struct_kddm_object *obj;
	unique_id_t id;

	/* Exclude kernel threads and local pids from using sighand_struct kddm
	 * objects. */
	/* At this stage, if need_update is false, task->mm points the mm of the
	 * task being duplicated instead of the mm of task for which this struct
	 * is being allocated, but we only need to know whether it is NULL or
	 * not, which will be the same after copy_mm. */
	if (!(task->pid & GLOBAL_PID_MASK) || !task->mm) {
		BUG_ON(krg_current);
		/* Let the vanilla mechanism allocate the sighand_struct. */
		return NULL;
	}

	if (!need_update || !task->sighand->krg_objid)
		id = get_unique_id(&sighand_struct_id_root);
	else
		id = task->sighand->krg_objid;

	// get the sighand object
	obj = _kddm_grab_object(sighand_struct_kddm_set, id);
	BUG_ON(!obj);
	DEBUG(DBG_SIGHAND, 2, "%lu (0x%p)\n", id, obj);

	if (need_update && !task->sighand->krg_objid) {
		/* Kerrighed transformation of first task using this sighand, or
		 * process executing execve and needing a new sighand because
		 * the former one is still shared with another process */
		kmem_cache_free(sighand_cachep, obj->sighand);
		task->sighand->krg_objid = id;
		task->sighand->kddm_obj = obj;
		obj->sighand = task->sighand;
		/* obj->count should already have been set to 1 by
		 * first_touch */
	} else if (unlikely(need_update))
		/* Kerrighed transformation of another task using this sighand
		 * struct */
		atomic_inc(&obj->count);

	kcb_sighand_struct_unlock(id);

	return obj->sighand;
}

struct sighand_struct *cr_malloc_sighand_struct(void)
{
	struct sighand_struct_kddm_object *obj;
	unique_id_t id;

	id = get_unique_id(&sighand_struct_id_root);

	// get the sighand object
	obj = _kddm_grab_object(sighand_struct_kddm_set, id);
	BUG_ON(!obj);
	DEBUG(DBG_SIGHAND, 2, "%lu (0x%p)\n", id, obj);

	return obj->sighand;
}

void cr_free_sighand_struct(objid_t id)
{
	_kddm_remove_frozen_object(sighand_struct_kddm_set, id);
}

/* Assumes that the associated kddm object is write locked.
 */
void kcb_share_sighand(struct task_struct *task)
{
	struct sighand_struct_kddm_object *obj;
	int count;

	if (!task->sighand->krg_objid)
		return;
	obj = _kddm_find_object(sighand_struct_kddm_set,
			       task->sighand->krg_objid);
	BUG_ON(!obj);
	count = atomic_inc_return(&obj->count);
	DEBUG(DBG_SIGHAND, 1, "%lu (0x%p), %d\n",
	      task->sighand->krg_objid, obj, count);
	_kddm_put_object(sighand_struct_kddm_set, task->sighand->krg_objid);
}

objid_t kcb_exit_sighand(objid_t id)
{
	struct sighand_struct_kddm_object *obj;
	int count;

	if (!id)
		return 0;
	obj = sighand_struct_writelock(id);
	BUG_ON(!obj);
	count = atomic_dec_return(&obj->count);
	DEBUG(DBG_SIGHAND, 1, "%lu (0x%p), %d\n", id, obj, count);
	if (count == 0) {
		kcb_sighand_struct_unlock(id);
		BUG_ON(obj->keep_on_remove);
		/* Free the kddm object but keep the sighand_struct so that
		 * __exit_sighand releases it properly. */
		obj->keep_on_remove = 1;
		_kddm_remove_object(sighand_struct_kddm_set, id);

		return 0;
	}

	return id;
}

void register_sighand_hooks(void)
{
	hook_register(&kh_sighand_struct_readlock, kcb_sighand_struct_readlock);
	hook_register(&kh_sighand_struct_writelock,
		      kcb_sighand_struct_writelock);
	hook_register(&kh_sighand_struct_unlock, kcb_sighand_struct_unlock);

	hook_register(&kh_malloc_sighand_struct, kcb_malloc_sighand_struct);
	hook_register(&kh_share_sighand, kcb_share_sighand);
	hook_register(&kh_exit_sighand, kcb_exit_sighand);

	hook_register(&kh_sighand_struct_pin, kcb_sighand_struct_pin);
	hook_register(&kh_sighand_struct_unpin, kcb_sighand_struct_unpin);
}

int proc_sighand_start(void)
{
	unsigned long cache_flags = SLAB_PANIC;

	DEBUG(DBG_SIGHAND, 1, "Starting the sighand manager...\n");

#ifdef CONFIG_DEBUG_SLAB
	cache_flags |= SLAB_POISON;
#endif
	sighand_struct_kddm_obj_cachep =
		kmem_cache_create("sighand_struct_kddm_obj",
				  sizeof(struct sighand_struct_kddm_object),
				  0, cache_flags,
				  NULL, NULL);

	/* Objid 0 is reserved to mark a sighand_struct having not been
	 * linked to a kddm object yet */
	init_and_set_unique_id_root(&sighand_struct_id_root, 1);

	register_io_linker(SIGHAND_STRUCT_LINKER, &sighand_struct_io_linker);

	sighand_struct_kddm_set =
		create_new_kddm_set(kddm_def_ns,
				    SIGHAND_STRUCT_KDDM_ID,
				    SIGHAND_STRUCT_LINKER,
				    KDDM_CUSTOM_DEF_OWNER,
				    0, 0);
	if (IS_ERR(sighand_struct_kddm_set))
		OOM;

	return 0;
}

void proc_sighand_exit(void)
{
}
