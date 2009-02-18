/*
 *  Kerrighed/modules/proc/task.c
 *
 *  Copyright (C) 1999-2006 INRIA, Universite de Rennes 1, EDF
 *  Copyright (C) 2006-2007 Pascal Gallard - Kerlabs, Louis Rilling - Kerlabs
 */

/** On each node the system manage a table to know the
 *  location of migrated process.
 *  It is interesting to globally manage signal : e.g. when a signal
 *  arrive from a remote node, the system can find the old local
 *  process pid and so the process'father.
 */

#include <linux/types.h>
#include <linux/module.h>
#include <linux/personality.h>
#include <linux/sched.h>
#include <linux/rwsem.h>
#include <linux/lockdep.h>
#include <linux/rcupdate.h>
#include <linux/kref.h>
#include <linux/slab.h>
#include <kerrighed/task.h>
#include <kerrighed/pid.h>

#include "debug_proc.h"

#define MODULE_NAME "task"

#include <rpc/rpc.h>
#include <hotplug/hotplug.h>
#include <ctnr/kddm.h>
#ifdef CONFIG_KRG_EPM
#include "pid.h"
#endif
#include "libproc.h"
#include "task.h"

static struct kmem_cache *task_kddm_obj_cachep;

/* kddm set of pid location and task struct */
static struct kddm_set *task_kddm_set = NULL;

static void kcb_task_get(struct task_kddm_object *obj)
{
	if (obj) {
		kref_get(&obj->kref);
		DEBUG(DBG_TASK_KDDM, 4, "%d count=%d\n",
		      obj->pid, atomic_read(&obj->kref.refcount));
	}
}

static void task_free(struct kref *kref)
{
	struct task_kddm_object *obj;

	obj = container_of(kref, struct task_kddm_object, kref);
	BUG_ON(!obj);

	DEBUG(DBG_TASK_KDDM, 2, "%d\n", obj->pid);

	kmem_cache_free(task_kddm_obj_cachep, obj);
}

static void kcb_task_put(struct task_kddm_object *obj)
{
	if (obj) {
		DEBUG(DBG_TASK_KDDM, 4, "%d count=%d\n",
		      obj->pid, atomic_read(&obj->kref.refcount));
		kref_put(&obj->kref, task_free);
	}
}

/*
 * @author Pascal Gallard
 */
static int task_alloc_object(struct kddm_obj *obj_entry,
			     struct kddm_set *ctnr, objid_t objid)
{
	struct task_kddm_object *p;
	struct task_struct *tsk;

	read_lock(&tasklist_lock);
	tsk = find_task_by_pid(objid);
	if (tsk && tsk->task_obj) {
		BUG();
		DEBUG(DBG_TASK_KDDM, 4, "existing %lu\n", objid);
		p = tsk->task_obj;
		read_unlock(&tasklist_lock);
	} else {
		read_unlock(&tasklist_lock);
		DEBUG(DBG_TASK_KDDM, 4, "non existing %lu\n", objid);
		p = kmem_cache_alloc(task_kddm_obj_cachep, GFP_KERNEL);
		if (p == NULL)
			return -ENOMEM;
	}

	DEBUG(DBG_TASK_KDDM, 3, "%lu 0x%p\n", objid, p);
	p->node = KERRIGHED_NODE_ID_NONE;
	p->task = NULL;
	p->pid = objid;
	p->parent_node = KERRIGHED_NODE_ID_NONE;
	p->group_leader = objid; /* If the group leader is another thread, this
				  * will be fixed later. Before that this is
				  * only needed to check local/global pids. */

#if 0
	p->krg_thread.waited_by = -1;
#endif
#ifdef CONFIG_KRG_EPM
	p->pid_obj = NULL;
#endif
	init_rwsem(&p->sem);

	p->alive = 1;
	kref_init(&p->kref);
	obj_entry->object = p;

	return 0;
}

/*
 * @author Pascal Gallard
 */
static int task_first_touch(struct kddm_obj *obj_entry,
			    struct kddm_set *ctnr, objid_t objid, int flags)
{
	return task_alloc_object(obj_entry, ctnr, objid);
}

/*
 * @author Pascal Gallard
 */
static int task_import_object(struct kddm_obj *obj_entry,
			      struct rpc_desc *desc)
{
	struct task_kddm_object *dest = obj_entry->object;
	struct task_kddm_object src;
	int retval;

	DEBUG(DBG_TASK_KDDM, 3, "%d dest=0x%p\n", dest->pid, dest);

	retval = rpc_unpack_type(desc, src);
	if (retval)
		return retval;

	write_lock_irq(&tasklist_lock);

	dest->state = src.state;
	dest->flags = src.flags;
	dest->ptrace = src.ptrace;
	dest->exit_state = src.exit_state;
	dest->exit_code = src.exit_code;
	dest->exit_signal = src.exit_signal;

	dest->node = src.node;
	dest->self_exec_id = src.self_exec_id;
	dest->thread_group_empty = src.thread_group_empty;

	dest->parent = src.parent;
	dest->parent_node = src.parent_node;
	dest->real_parent = src.real_parent;
	dest->real_parent_tgid = src.real_parent_tgid;
	dest->group_leader = src.group_leader;

	dest->uid = src.uid;
	dest->euid = src.euid;
	dest->egid = src.egid;

	dest->utime = src.utime;
	dest->stime = src.stime;

	dest->dumpable = src.dumpable;

	write_unlock_irq(&tasklist_lock);

	return 0;
}

/* Assumes either tasklist_lock read locked with appropriate task_lock held, or
 * tasklist_lock write locked.
 */
static void task_update_object(struct task_kddm_object *obj)
{
	struct task_struct *tsk = obj->task;

	if (tsk) {
		BUG_ON(tsk->task_obj != obj);

		obj->state = tsk->state;
		obj->flags = tsk->flags;
		obj->ptrace = tsk->ptrace;
		obj->exit_state = tsk->exit_state;
		obj->exit_code = tsk->exit_code;
		obj->exit_signal = tsk->exit_signal;

		obj->self_exec_id = tsk->self_exec_id;

		BUG_ON(obj->node != kerrighed_node_id &&
		       obj->node != KERRIGHED_NODE_ID_NONE);

		obj->uid = tsk->uid;
		obj->euid = tsk->euid;
		obj->egid = tsk->egid;

		obj->utime = tsk->utime;
		obj->stime = tsk->stime;

		obj->dumpable = (tsk->mm && tsk->mm->dumpable == 1);

		obj->thread_group_empty = thread_group_empty(tsk);
	}
}

/*
 * @author Pascal Gallard
 */
static int task_export_object(struct rpc_desc *desc,
			      struct kddm_obj *obj_entry)
{
	struct task_kddm_object *src = obj_entry->object;
	struct task_struct *tsk;

	DEBUG(DBG_TASK_KDDM, 3, "%d src=0x%p\n", src->pid, src);

	read_lock(&tasklist_lock);
	tsk = src->task;
	if (likely(tsk)) {
		task_lock(tsk);
		task_update_object(src);
		task_unlock(tsk);
	}
	read_unlock(&tasklist_lock);

	return rpc_pack_type(desc, *src);
}

static void delayed_task_put(struct rcu_head *rhp)
{
	struct task_kddm_object *obj =
		container_of(rhp, struct task_kddm_object, rcu);

	kcb_task_put(obj);
}

/**
 *  @author Louis Rilling
 */
static int task_remove_object(void *object,
			      struct kddm_set *ctnr, objid_t objid)
{
	struct task_kddm_object *obj = object;

	DEBUG(DBG_TASK_KDDM, 3, "%d 0x%p\n", obj->pid, obj);

	kcb_task_unlink(obj, 0);

	rcu_read_lock();
	pid_unlink_task(rcu_dereference(obj->pid_obj));
	rcu_read_unlock();
	BUG_ON(obj->pid_obj);

	obj->alive = 0;
	call_rcu(&obj->rcu, delayed_task_put);

	return 0;
}

static struct iolinker_struct task_io_linker = {
	.first_touch   = task_first_touch,
	.linker_name   = "task ",
	.linker_id     = TASK_LINKER,
	.alloc_object  = task_alloc_object,
	.export_object = task_export_object,
	.import_object = task_import_object,
	.remove_object = task_remove_object,
	.default_owner = global_pid_default_owner,
};

static struct task_struct * kcb_malloc_task_struct(pid_t pid)
{
	struct task_struct *tsk;

#ifdef CONFIG_KRG_DEBUG
	rcu_read_lock();
	tsk = find_task_by_pid(pid);
	BUG_ON(tsk);
	rcu_read_unlock();
#endif

	tsk = alloc_task_struct();
	if (!tsk)
		return NULL;

	/* Exclude kernel threads and local pids from using task kddm objects. */
	/* At this stage, current->mm points the mm of the task being duplicated
	 * instead of the mm of task for which this struct is being allocated,
	 * but we only need to know whether it is NULL or not, which will be the
	 * same after copy_mm. */
	if (!(pid & GLOBAL_PID_MASK) || !current->mm) {
#ifdef CONFIG_KRG_EPM
		BUG_ON(krg_current);
#endif
		tsk->task_obj = NULL;
		return tsk;
	}

	/* Needed if the task kddm object must be freed in
	 * dup_task_struct. */
	tsk->pid = pid;

#ifdef CONFIG_KRG_EPM
	if (krg_current == NULL) {
#endif
		/* We are in a regular fork (not a migration fork) so we can
		 * grab everythings we want */

		/* In this case and only in this case, we will grab our children
		 * object and later our parent's one while still holding the
		 * write lock on this task kddm object. There is no risk of
		 * deadlock since this object will be known only after having
		 * grabbed and released the other children objects locks. */
		lockdep_off();
		tsk->task_obj = kcb_task_create_writelock(pid);
		lockdep_on();
		if (likely(tsk->task_obj))
			/* Set the link between task kddm object and tsk */
			tsk->task_obj->task = tsk;
#ifdef CONFIG_KRG_EPM
	} else {
		tsk->task_obj = NULL;
	}
#endif

	return tsk;
}

void kcb___free_task_struct(struct task_struct *task)
{
	DEBUG(DBG_TASK_KDDM, 2, "%d\n", task->pid);
	_kddm_remove_object(task_kddm_set, task->pid);
}

static void kcb_free_task_struct(struct task_struct *task)
{
	/* If the pointer is NULL and the object exists, this is a BUG! */
	if (!task->task_obj)
		return;

	kcb___free_task_struct(task);
}

/* Expects tasklist write locked
 */
void __kcb_task_unlink(struct task_kddm_object *obj, int need_update)
{
	BUG_ON(!obj);

	DEBUG(DBG_TASK_KDDM, 2, "%d\n", obj->pid);
	if (obj->task) {
		if (need_update)
			task_update_object(obj);
		rcu_assign_pointer(obj->task->task_obj, NULL);
		rcu_assign_pointer(obj->task, NULL);
	}
}

void kcb_task_unlink(struct task_kddm_object *obj, int need_update)
{
	write_lock_irq(&tasklist_lock);
	__kcb_task_unlink(obj, need_update);
	write_unlock_irq(&tasklist_lock);
}

static int kcb_task_alive(struct task_kddm_object *obj)
{
	return obj && obj->alive;
}

/**
 * @author Pascal Gallard
 */
struct task_kddm_object * kcb_task_readlock(pid_t pid)
{
	struct task_kddm_object *obj;

	/* Filter well known cases of no task kddm object. */
	if (!(pid & GLOBAL_PID_MASK))
		return NULL;

	obj = _kddm_get_object_no_ft(task_kddm_set, pid);
	if (likely(obj)) {
		down_read(&obj->sem);
		/* Marker for unlock. Dirty but temporary. */
		obj->write_locked = 0;
	}
	DEBUG(DBG_TASK_KDDM, 2, "%d (0x%p)\n", pid, obj);

	return obj;
}

/**
 * @author Pascal Gallard
 */
static struct task_kddm_object * task_writelock(pid_t pid, int nested)
{
	struct task_kddm_object *obj;

	/* Filter well known cases of no task kddm object. */
	if (!(pid & GLOBAL_PID_MASK))
		return NULL;

	obj = _kddm_grab_object_no_ft(task_kddm_set, pid);
	if (likely(obj)) {
		if (!nested)
			down_write(&obj->sem);
		else
			down_write_nested(&obj->sem, SINGLE_DEPTH_NESTING);
		/* Marker for unlock. Dirty but temporary. */
		obj->write_locked = 1;
	}
	DEBUG(DBG_TASK_KDDM, 2, "%d (0x%p)\n", pid, obj);

	return obj;
}

struct task_kddm_object * kcb_task_writelock(pid_t pid)
{
	return task_writelock(pid, 0);
}

static struct task_kddm_object * kcb_task_writelock_nested(pid_t pid)
{
	return task_writelock(pid, 1);
}

/**
 * @author Louis Rilling
 */
struct task_kddm_object * kcb_task_create_writelock(pid_t pid)
{
	struct task_kddm_object *obj;

	/* Filter well known cases of no task kddm object. */
	/* The exact filter is expected to be implemented by the caller. */
	BUG_ON(!(pid & GLOBAL_PID_MASK));

	obj = _kddm_grab_object(task_kddm_set, pid);
	if (likely(obj && !IS_ERR(obj))) {
		down_write(&obj->sem);
		/* Marker for unlock. Dirty but temporary. */
		obj->write_locked = 1;
	} else {
		_kddm_put_object(task_kddm_set, pid);
	}
	DEBUG(DBG_TASK_KDDM, 2, "%d (0x%p)\n", pid, obj);

	return obj;
}

/**
 * @author Pascal Gallard
 */
void kcb_task_unlock(pid_t pid)
{
	/* Filter well known cases of no task kddm object. */
	if (!(pid & GLOBAL_PID_MASK))
		return;

	DEBUG(DBG_TASK_KDDM, 2, "%d\n", pid);
	{
		/* Dirty tricks here. Hopefully it should be temporary waiting
		 * for kddm to implement locking on a task basis. */
		struct task_kddm_object *obj;

		obj = _kddm_find_object(task_kddm_set, pid);
		if (likely(obj)) {
			_kddm_put_object(task_kddm_set, pid);
			if (obj->write_locked)
				up_write(&obj->sem);
			else
				up_read(&obj->sem);
		}
	}
	_kddm_put_object(task_kddm_set, pid);
}

int kcb_fill_task_kddm_object(struct task_struct *task)
{
	task->task_obj = kcb_task_create_writelock(task->pid);
	if (likely(task->task_obj)) {
		task->task_obj->node = kerrighed_node_id;
		task->task_obj->task = task;

		task->task_obj->parent = task->parent->pid;
		task->task_obj->real_parent = task->real_parent->pid;
		task->task_obj->real_parent_tgid = task->real_parent->tgid;
		task->task_obj->group_leader = task->group_leader->pid;

		kcb_task_unlock(task->pid);
	}

	return 0;
}

/**
 * @author Pascal Gallard
 * Set (or update) the location of pid
 */
int kcb_set_pid_location(pid_t pid, kerrighed_node_t node)
{
	struct task_kddm_object *p;

	/* Filter well known cases of no task kddm object. */
	if (!(pid & GLOBAL_PID_MASK))
		return 0;

	p = kcb_task_writelock(pid);
	if (likely(p))
		p->node = node;
	kcb_task_unlock(pid);

	return 0;
}

int kcb_unset_pid_location(pid_t pid)
{
	struct task_kddm_object *p;

	BUG_ON(!(pid & GLOBAL_PID_MASK));

	p = kcb_task_writelock(pid);
	BUG_ON(p == NULL);
	p->node = KERRIGHED_NODE_ID_NONE;
	kcb_task_unlock(pid);

	return 0;
}

kerrighed_node_t kcb_lock_pid_location(pid_t pid)
{
	kerrighed_node_t node = KERRIGHED_NODE_ID_NONE;
	struct task_kddm_object *obj;
#ifdef CONFIG_KRG_EPM
	struct timespec back_off_time = {
		.tv_sec = 0,
		.tv_nsec = 1000000 /* 1 ms */
	};
#endif

	for (;;) {
		obj = kcb_task_readlock(pid);
		if (likely(obj))
			node = obj->node;
		else {
			kcb_task_unlock(pid);
			break;
		}
#ifdef CONFIG_KRG_EPM
		if (likely(node != KERRIGHED_NODE_ID_NONE))
			break;
		DEBUG(DBG_TASK_KDDM, 4, "%s node=%d, backing off\n",
		      current->comm, node);
		/* Task is migrating.
		 * Back off and hope that it will stop migrating. */
		kcb_task_unlock(pid);
		set_current_state(TASK_UNINTERRUPTIBLE);
		schedule_timeout(timespec_to_jiffies(&back_off_time) + 1);
#else
		break;
#endif
	}

	return node;
}

void kcb_unlock_pid_location(pid_t pid)
{
	kcb_task_unlock(pid);
}

void register_task_hooks(void)
{
	hook_register(&kh_task_get, kcb_task_get);
	hook_register(&kh_task_put, kcb_task_put);
	hook_register(&kh_task_alive, kcb_task_alive);
	hook_register(&kh_task_unlock, kcb_task_unlock);
	hook_register(&kh_task_readlock, kcb_task_readlock);
	hook_register(&kh_task_writelock, kcb_task_writelock);
	hook_register(&kh_task_writelock_nested, kcb_task_writelock_nested);
	hook_register(&kh_malloc_task_struct, kcb_malloc_task_struct);
	hook_register(&kh_free_task_struct, kcb_free_task_struct);

	hook_register(&kh_set_pid_location, kcb_set_pid_location);
#ifdef CONFIG_KRG_EPM
	hook_register(&kh_unset_pid_location, kcb_unset_pid_location);
#endif
	hook_register(&kh_lock_pid_location, kcb_lock_pid_location);
	hook_register(&kh_unlock_pid_location, kcb_unlock_pid_location);
}

/**
 * @author David Margery
 * @author Pascal Gallard (update to kddm architecture)
 * @author Louis Rilling (split files)
 */
void proc_task_start(void)
{
	unsigned long cache_flags = SLAB_PANIC;

#ifdef CONFIG_DEBUG_SLAB
	cache_flags |= SLAB_POISON;
#endif
	task_kddm_obj_cachep =
		kmem_cache_create("task_kddm_obj",
				  sizeof(struct task_kddm_object),
				  0, cache_flags,
				  NULL, NULL);

	register_io_linker(TASK_LINKER, &task_io_linker);

	task_kddm_set = create_new_kddm_set(kddm_def_ns, TASK_KDDM_ID,
					    TASK_LINKER,
					    KDDM_CUSTOM_DEF_OWNER,
					    0, 0);
	if (IS_ERR(task_kddm_set))
		OOM;

	DEBUG(DBG_TASK_KDDM, 1, "Done\n");
}

/**
 * @author David Margery
 * @author Pascal Gallard (update to kddm architecture)
 * @author Louis Rilling (split files)
 */
void proc_task_exit(void)
{
}
