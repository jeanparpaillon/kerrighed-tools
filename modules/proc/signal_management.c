/*
 *  Kerrighed/modules/proc/signal_management.c
 *
 *  Copyright (C) 1999-2006 INRIA, Universite de Rennes 1, EDF
 *  Copyright (C) 2006-2007 Pascal Gallard - Kerlabs, Louis Rilling - Kerlabs
 */

#include <linux/types.h>
#include <linux/sched.h>
#include <linux/slab.h>
#ifdef CONFIG_TASKSTATS
#include <linux/taskstats.h>
#include <linux/taskstats_kern.h>
#endif
#include <linux/rwsem.h>
#include <kerrighed/signal.h>
#include <kerrighed/pid.h>

#include "debug_proc.h"

#define MODULE_NAME "signal"

#include <rpc/rpc.h>
#include <hotplug/hotplug.h>
#include <ctnr/kddm.h>
#include "libproc.h"
#include "signal_management.h"

struct signal_struct_kddm_object {
	struct signal_struct *signal;
	atomic_t count;
	int keep_on_remove;
	struct rw_semaphore remove_sem;
};

static struct kmem_cache *signal_struct_kddm_obj_cachep;

/* Kddm set of 'struct signal_struct' */
static struct kddm_set *signal_struct_kddm_set = NULL;

/*
 * @author Pascal Gallard
 */
static int signal_struct_alloc_object(struct kddm_obj *obj_entry,
				      struct kddm_set *set, objid_t objid)
{
	struct signal_struct_kddm_object *obj;
	struct signal_struct *sig;

	obj = kmem_cache_alloc(signal_struct_kddm_obj_cachep, GFP_KERNEL);
	DEBUG(DBG_SIGNAL, 3, "%lu (0x%p)\n", objid, obj);
	if (!obj)
		return -ENOMEM;

	sig = kmem_cache_alloc(signal_cachep, GFP_KERNEL);
	if (!sig) {
		kmem_cache_free(signal_struct_kddm_obj_cachep, obj);
		return -ENOMEM;
	}

/* 	atomic_set(&obj->signal->count, 1); */
/* 	atomic_set(&obj->signal->live, 1); */
	init_waitqueue_head(&sig->wait_chldexit);
/* 	obj->signal->flags = 0; */
/* 	obj->signal->group_exit_code = 0; */
	sig->group_exit_task = NULL;
/* 	obj->signal->group_stop_count = 0; */
	sig->curr_target = NULL;
	init_sigpending(&sig->shared_pending);
	INIT_LIST_HEAD(&sig->posix_timers);

	hrtimer_init(&sig->real_timer, CLOCK_MONOTONIC, HRTIMER_REL);
	sig->tsk = NULL;

	INIT_LIST_HEAD(&sig->cpu_timers[0]);
	INIT_LIST_HEAD(&sig->cpu_timers[1]);
	INIT_LIST_HEAD(&sig->cpu_timers[2]);

	sig->tty = NULL;
/* 	obj->signal->leader = 0;	/\* session leadership doesn't inherit *\/ */
/* 	obj->signal->session = 0; */
/* 	obj->signal->pgrp = 0; */
/* 	obj->signal->tty_old_pgrp = 0; */
#ifdef CONFIG_KEYS
	sig->session_keyring = NULL;
	sig->process_keyring = NULL;
#endif
#ifdef CONFIG_TASKSTATS
	sig->stats = NULL;
#endif
	sig->krg_objid = objid;
	sig->kddm_obj = obj;
	obj->signal = sig;

/* 	atomic_set(&obj->count, 1); */
	obj->keep_on_remove = 0;
	init_rwsem(&obj->remove_sem);
	obj_entry->object = obj;

	return 0;
}

/*
 * @author Pascal Gallard
 */
static int signal_struct_first_touch(struct kddm_obj *obj_entry,
				     struct kddm_set *set, objid_t objid,
				     int flags)
{
	struct signal_struct_kddm_object *obj;
	int r;

	r = signal_struct_alloc_object(obj_entry, set, objid);
	if (r)
		return r;
	DEBUG(DBG_SIGNAL, 3, "%lu (0x%p)\n", objid, obj_entry->object);

	obj = obj_entry->object;
	atomic_set(&obj->count, 1);

	return 0;
}

/*
 * Lock on the dest signal_struct must be held. No other access
 * to dest is allowed in the import time.
 * @author Pascal Gallard
 */
static int signal_struct_import_object(struct kddm_obj *obj_entry,
				       struct rpc_desc *desc)
{
	struct signal_struct_kddm_object *obj = obj_entry->object;
	struct signal_struct *dest = obj->signal;
	struct signal_struct tmp_sig;
	int retval;

	DEBUG(DBG_SIGNAL, 3, "%lu (0x%p)\n", obj->signal->krg_objid, obj);

	retval = rpc_unpack_type(desc, tmp_sig);
	if (retval)
		return retval;
#ifdef CONFIG_TASKSTATS
	if (tmp_sig.stats) {
		retval = -ENOMEM;
		tmp_sig.stats = kmem_cache_alloc(taskstats_cache, GFP_KERNEL);
		if (!tmp_sig.stats)
			return retval;
		retval = rpc_unpack_type(desc, *tmp_sig.stats);
		if (retval) {
			kmem_cache_free(taskstats_cache, tmp_sig.stats);
			return retval;
		}
	}
#endif
	retval = rpc_unpack_type(desc, obj->count);
	if (retval)
		return retval;

	/* We are only modifying a copy of the real signal struct. All pointers
	 * should be left NULL. */
	/* TODO: with distributed threads this will need more locking */
	atomic_set(&dest->count, atomic_read(&tmp_sig.count));
	atomic_set(&dest->live, atomic_read(&tmp_sig.live));

	dest->group_exit_code = tmp_sig.group_exit_code;
	dest->notify_count = tmp_sig.notify_count;
	dest->group_stop_count = tmp_sig.group_stop_count;
	dest->flags = tmp_sig.flags;

	dest->it_real_incr = tmp_sig.it_real_incr;
	dest->it_prof_expires = tmp_sig.it_prof_expires;
	dest->it_virt_expires = tmp_sig.it_virt_expires;
	dest->it_prof_incr = tmp_sig.it_prof_incr;
	dest->it_virt_incr = tmp_sig.it_virt_incr;

	dest->pgrp = tmp_sig.pgrp;
	dest->tty_old_pgrp = tmp_sig.tty_old_pgrp;
	dest->__session = tmp_sig.__session;
	dest->leader = tmp_sig.leader;

	dest->utime = tmp_sig.utime;
	dest->stime = tmp_sig.stime;
	dest->cutime = tmp_sig.cutime;
	dest->cstime = tmp_sig.cstime;
	dest->nvcsw = tmp_sig.nvcsw;
	dest->nivcsw = tmp_sig.nivcsw;
	dest->cnvcsw = tmp_sig.cnvcsw;
	dest->cnivcsw = tmp_sig.cnivcsw;
	dest->min_flt = tmp_sig.min_flt;
	dest->maj_flt = tmp_sig.maj_flt;
	dest->cmin_flt = tmp_sig.cmin_flt;
	dest->cmaj_flt = tmp_sig.cmaj_flt;
	dest->sched_time = tmp_sig.sched_time;

	memcpy(dest->rlim, tmp_sig.rlim, sizeof(dest->rlim));
#ifdef CONFIG_BSD_PROCESS_ACCT
	dest->pacct = tmp_sig.pacct;
#endif
#ifdef CONFIG_TASKSTATS
	if (tmp_sig.stats) {
		if (dest->stats) {
			memcpy(dest->stats, tmp_sig.stats,
			       sizeof(*dest->stats));
			kmem_cache_free(taskstats_cache, tmp_sig.stats);
		} else {
			dest->stats = tmp_sig.stats;
		}
	}
#endif

	return 0;
}

/*
 * @author Pascal Gallard
 */
static int signal_struct_export_object(struct rpc_desc *desc,
				       struct kddm_obj *obj_entry)
{
	struct signal_struct_kddm_object *obj = obj_entry->object;
	struct task_struct *tsk;
	unsigned long flags;
	int retval;

	DEBUG(DBG_SIGNAL, 3, "%lu (0x%p)\n", obj->signal->krg_objid, obj);

	rcu_read_lock();
	tsk = find_task_by_pid(obj->signal->krg_objid);
	/*
	 * We may find no task in the middle of a migration. In that case, kddm
	 * locking is enough since neither userspace nor the kernel will access
	 * this copy.
	 */
	if (tsk)
		get_task_struct(tsk);
	rcu_read_unlock();
	if (tsk && !lock_task_sighand(tsk, &flags))
		BUG();
	retval = rpc_pack_type(desc, *obj->signal);
#ifdef CONFIG_TASKSTATS
	if (!retval && obj->signal->stats)
		retval = rpc_pack_type(desc, *obj->signal->stats);
#endif
	if (tsk) {
		unlock_task_sighand(tsk, &flags);
		put_task_struct(tsk);
	}
	if (!retval)
		retval = rpc_pack_type(desc, obj->count);

	return retval;
}

void kcb_signal_struct_pin(struct signal_struct *sig)
{
	struct signal_struct_kddm_object *obj = sig->kddm_obj;
	BUG_ON(!obj);
	down_read(&obj->remove_sem);
}

void kcb_signal_struct_unpin(struct signal_struct *sig)
{
	struct signal_struct_kddm_object *obj = sig->kddm_obj;
	BUG_ON(!obj);
	up_read(&obj->remove_sem);
}

static int signal_struct_remove_object(void *object,
				       struct kddm_set *set, objid_t objid)
{
	struct signal_struct_kddm_object *obj = object;

	/* Ensure that no thread uses this signal_struct copy */
	down_write(&obj->remove_sem);
	up_write(&obj->remove_sem);

	DEBUG(DBG_SIGNAL, 3, "%lu (0x%p), keep=%d, %d\n",
	      objid, obj, obj->keep_on_remove,
	      atomic_read(&obj->signal->count));

	if (!obj->keep_on_remove) {
		struct signal_struct *sig = obj->signal;

		WARN_ON(!list_empty(&sig->shared_pending.list));
		flush_sigqueue(&sig->shared_pending);
#ifdef CONFIG_TASKSTATS
		taskstats_tgid_free(sig);
#endif
		kmem_cache_free(signal_cachep, sig);
	}
	kmem_cache_free(signal_struct_kddm_obj_cachep, obj);

	return 0;
}

static struct iolinker_struct signal_struct_io_linker = {
	.first_touch   = signal_struct_first_touch,
	.linker_name   = "sig ",
	.linker_id     = SIGNAL_STRUCT_LINKER,
	.alloc_object  = signal_struct_alloc_object,
	.export_object = signal_struct_export_object,
	.import_object = signal_struct_import_object,
	.remove_object = signal_struct_remove_object,
	.default_owner = global_pid_default_owner,
};

struct signal_struct *cr_malloc_signal_struct(pid_t tgid)
{
	struct signal_struct_kddm_object *obj;
	objid_t id;

	id = tgid;

	/* get the signal object */
	obj = _kddm_grab_object(signal_struct_kddm_set, id);
	BUG_ON(!obj);
	DEBUG(DBG_SIGNAL, 2, "%d (0x%p)\n", tgid, obj);

	return obj->signal;
}

void cr_free_signal_struct(pid_t id)
{
	_kddm_remove_frozen_object(signal_struct_kddm_set, id);
}

/*
 * Alloc a dedicated signal_struct to task_struct task.
 * @author Pascal Gallard
 */
struct signal_struct *kcb_malloc_signal_struct(struct task_struct *task,
					       int need_update)
{
	struct signal_struct_kddm_object *obj;
	objid_t id;

	/* Exclude kernel threads and local pids from using signal_struct
	 * kddm objects. */
	/* At this stage, if need_update is false, task->mm points the mm of the
	 * task being duplicated instead of the mm of task for which this struct
	 * is being allocated, but we only need to know whether it is NULL or
	 * not, which will be the same after copy_mm. */
	if (!(task->tgid & GLOBAL_PID_MASK) || !task->mm) {
#ifdef CONFIG_KRG_EPM
		BUG_ON(krg_current);
#endif
		/* Let the vanilla mechanism allocate the signal_struct. */
		return NULL;
	}

	id = task->tgid;

	/* get the signal object */
	obj = _kddm_grab_object(signal_struct_kddm_set, id);
	BUG_ON(!obj);
	DEBUG(DBG_SIGNAL, 2, "%d (0x%p)\n", task->tgid, obj);

	if (unlikely(need_update && !task->signal->krg_objid)) {
		/* Transformation of first thread of a thread group
		 */
		kmem_cache_free(signal_cachep, obj->signal);
		task->signal->krg_objid = id;
		task->signal->kddm_obj = obj;
		obj->signal = task->signal;
		atomic_set(&obj->count, atomic_read(&task->signal->count));
	}

	return obj->signal;
}

/*
 * Get and lock a signal structure for a given process
 * @author Pascal Gallard
 */
struct signal_struct *kcb_signal_struct_readlock(pid_t tgid)
{
	struct signal_struct_kddm_object *obj;

	/* Filter well known cases of no signal_struct kddm object. */
	if (!(tgid & GLOBAL_PID_MASK))
		return NULL;

	obj = _kddm_get_object_no_ft(signal_struct_kddm_set, tgid);
	DEBUG(DBG_SIGNAL, 2, "%d (0x%p)\n", tgid, obj);
	if (!obj)
		return NULL;

	return obj->signal;
}

static struct signal_struct_kddm_object *signal_struct_writelock(pid_t tgid)
{
	struct signal_struct_kddm_object *obj;

	/* Filter well known cases of no signal_struct kddm object. */
	if (!(tgid & GLOBAL_PID_MASK))
		return NULL;

	obj = _kddm_grab_object_no_ft(signal_struct_kddm_set, tgid);
	DEBUG(DBG_SIGNAL, 2, "%d (0x%p)\n", tgid, obj);
	return obj;
}

/*
 * Grab and lock a signal structure for a given process
 * @author Pascal Gallard
 */
struct signal_struct *kcb_signal_struct_writelock(pid_t tgid)
{
	struct signal_struct_kddm_object *obj;

	/* Filter well known cases of no signal_struct kddm object. */
	if (!(tgid & GLOBAL_PID_MASK))
		return NULL;

	obj = signal_struct_writelock(tgid);
	if (!obj)
		return NULL;

	return obj->signal;
}

/*
 * unlock a signal structure for a given process
 * @author Pascal Gallard
 */
void kcb_signal_struct_unlock(pid_t tgid)
{
	/* Filter well known cases of no signal_struct kddm object. */
	if (!(tgid & GLOBAL_PID_MASK))
		return;

	DEBUG(DBG_SIGNAL, 2, "%d\n", tgid);
	_kddm_put_object(signal_struct_kddm_set, tgid);
}

/* Assumes that the associated kddm object is write locked.
 */
void kcb_share_signal(struct task_struct *task)
{
	struct signal_struct_kddm_object *obj;
	int count;

	/* Since we have the task_struct, we know exactly if the object
	 * exists. */
	if (!task->signal->krg_objid)
		return;

	obj = _kddm_find_object(signal_struct_kddm_set, task->tgid);
	BUG_ON(!obj);
	count = atomic_inc_return(&obj->count);
	DEBUG(DBG_SIGNAL, 1, "%d (0x%p), %d\n", task->tgid, obj, count);
	_kddm_put_object(signal_struct_kddm_set, task->tgid);
}

pid_t __kcb_exit_signal(pid_t id)
{
	struct signal_struct_kddm_object *obj;
	int count;

	/* We must not look at task->tgid since due to de_thread task may be
	 * the former group leader and have got a different tgid. */
	if (!id)
		return 0;

	obj = signal_struct_writelock(id);
	BUG_ON(!obj);
	count = atomic_dec_return(&obj->count);
	DEBUG(DBG_SIGNAL, 1, "%d (0x%p), %d\n", id, obj, count);
	if (count == 0) {
		kcb_signal_struct_unlock(id);
		BUG_ON(obj->keep_on_remove);
		/* Free the kddm object but keep the signal_struct so that
		 * __exit_signal releases it properly. */
		obj->keep_on_remove = 1;
		_kddm_remove_object(signal_struct_kddm_set, id);

		return 0;
	}

	return id;
}

pid_t kcb_exit_signal(struct task_struct *task)
{
	return __kcb_exit_signal(task->signal->krg_objid);
}

void register_signal_hooks(void)
{
	hook_register(&kh_signal_struct_readlock, kcb_signal_struct_readlock);
	hook_register(&kh_signal_struct_writelock, kcb_signal_struct_writelock);
	hook_register(&kh_signal_struct_unlock, kcb_signal_struct_unlock);

	hook_register(&kh_malloc_signal_struct, kcb_malloc_signal_struct);
	hook_register(&kh_share_signal, kcb_share_signal);
	hook_register(&kh_exit_signal, kcb_exit_signal);

	hook_register(&kh_signal_struct_pin, kcb_signal_struct_pin);
	hook_register(&kh_signal_struct_unpin, kcb_signal_struct_unpin);
}

int proc_signal_start(void)
{
	unsigned long cache_flags = SLAB_PANIC;

	DEBUG(DBG_SIGNAL, 1, "Starting the signal manager...\n");

#ifdef CONFIG_DEBUG_SLAB
	cache_flags |= SLAB_POISON;
#endif
	signal_struct_kddm_obj_cachep =
		kmem_cache_create("signal_struct_kddm_obj",
				  sizeof(struct signal_struct_kddm_object),
				  0, cache_flags,
				  NULL, NULL);

	register_io_linker(SIGNAL_STRUCT_LINKER, &signal_struct_io_linker);

	signal_struct_kddm_set = create_new_kddm_set(kddm_def_ns,
						     SIGNAL_STRUCT_KDDM_ID,
						     SIGNAL_STRUCT_LINKER,
						     KDDM_CUSTOM_DEF_OWNER,
						     0,
						     KDDM_LOCAL_EXCLUSIVE);
	if (IS_ERR(signal_struct_kddm_set))
		OOM;

	return 0;
}

void proc_signal_exit(void)
{
	return;
}
