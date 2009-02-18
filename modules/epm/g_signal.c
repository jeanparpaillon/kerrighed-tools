/*
 *  Kerrighed/modules/epm/g_signal.c
 *
 *  Copyright (C) 1999-2006 INRIA, Universite de Rennes 1, EDF
 *  Copyright (C) 2006-2007 Pascal Gallard - Kerlabs, Louis Rilling - Kerlabs
 */

/** Migration & checkpoint/restart of the signal informations process.
  *  @file g_signal.c
  *
  *  Implementation of the functions of migration for the signal informations of
  *  the process.
  *
  *  @author Geoffroy Vallée
  */

#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/hrtimer.h>
#include <linux/posix-timers.h>
#include <linux/taskstats_kern.h>
#include <kerrighed/signal.h>
#include <kerrighed/task.h>
#include "debug_epm.h"

#define MODULE_NAME "signal"

#include <ghost/ghost.h>
#include <proc/signal_management.h>
#include <epm/application/app_shared.h>
#include "action.h"

/* individual struct sigpending */

static int export_sigqueue(ghost_t *ghost, struct sigqueue *sig)
{
	int err;

	err = ghost_write(ghost, &sig->info, sizeof(sig->info));
	if (err)
		goto out;
	err = ghost_write(ghost, &sig->user->uid, sizeof(sig->user->uid));

out:
	return err;
}

static int import_sigqueue(ghost_t *ghost, struct sigqueue *sig)
{
	struct user_struct *user;
	uid_t uid;
	int err;

	err = ghost_read(ghost, &sig->info, sizeof(sig->info));
	if (err)
		goto out;

	err = ghost_read(ghost, &uid, sizeof(uid));
	if (err)
		goto out;
	user = alloc_uid(uid);
	if (!user) {
		err = -ENOMEM;
		goto out;
	}
	atomic_inc(&user->sigpending);

	atomic_dec(&sig->user->sigpending);
	free_uid(sig->user);

	sig->user = user;

out:
	return err;
}

static int export_sigpending(ghost_t *ghost,
			     struct task_struct *task,
			     struct sigpending *pending)
{
	struct sigpending tmp_queue;
	int nr_sig;
	struct sigqueue *q;
	unsigned long flags;
	int err;

	INIT_LIST_HEAD(&tmp_queue.list);
	nr_sig = 0;
	if (!lock_task_sighand(task, &flags))
		BUG();
	tmp_queue.signal = pending->signal;
	list_for_each_entry(q, &pending->list, list) {
		if (q->flags & SIGQUEUE_PREALLOC) {
			unlock_task_sighand(task, &flags);
			err = -EBUSY;
			goto out;
		}
		nr_sig++;
	}
	list_splice_init(&pending->list, &tmp_queue.list);
	unlock_task_sighand(task, &flags);

	err = ghost_write(ghost, &tmp_queue.signal, sizeof(tmp_queue.signal));
	if (err)
		goto out_splice;

	err = ghost_write(ghost, &nr_sig, sizeof(nr_sig));
	if (err)
		goto out_splice;

	list_for_each_entry(q, &tmp_queue.list, list) {
		err = export_sigqueue(ghost, q);
		if (err)
			goto out_splice;
	}

out_splice:
	if (!lock_task_sighand(task, &flags))
		BUG();
	sigorsets(&pending->signal, &pending->signal, &tmp_queue.signal);
	list_splice(&tmp_queue.list, &pending->list);
	recalc_sigpending_tsk(task);
	unlock_task_sighand(task, &flags);

out:
	return err;
}

static int import_sigpending(ghost_t *ghost,
			     struct task_struct *task,
			     struct sigpending *pending)
{
	int nr_sig;
	struct sigqueue *q;
	int i;
	int err;

	err = ghost_read(ghost, &pending->signal, sizeof(pending->signal));
	if (err)
		goto cleanup_queue;

	err = ghost_read(ghost, &nr_sig, sizeof(nr_sig));
	if (err)
		goto cleanup_queue;

	INIT_LIST_HEAD(&pending->list);
	for (i = 0; i < nr_sig; i++) {
		q = __sigqueue_alloc(current, GFP_KERNEL, 0);
		if (!q) {
			err = -ENOMEM;
			goto free_queue;
		}
		err = import_sigqueue(ghost, q);
		if (err) {
			__sigqueue_free(q);
			goto free_queue;
		}
		list_add_tail(&q->list, &pending->list);
	}

out:
	return err;

cleanup_queue:
	init_sigpending(pending);
	goto out;

free_queue:
	flush_sigqueue(pending);
	goto out;
}

static void unimport_sigpending(struct task_struct *task,
				struct sigpending *pending)
{
	flush_sigqueue(pending);
}

/* shared signals (struct signal_struct) */

static int export_posix_timers(ghost_t *ghost, struct task_struct *task)
{
	int err = 0;
	spin_lock_irq(&task->sighand->siglock);
	if (!list_empty(&task->signal->posix_timers))
		err = -EBUSY;
	spin_unlock_irq(&task->sighand->siglock);
	return err;
}

static int import_posix_timers(ghost_t *ghost, struct task_struct *task)
{
	BUG_ON(!list_empty(&task->signal->posix_timers));
	return 0;
}

static void unimport_posix_timers(struct task_struct *task)
{
}

#ifdef CONFIG_TASKSTATS
static int cr_export_taskstats(ghost_t *ghost, struct signal_struct *sig)
{
	return ghost_write(ghost, sig->stats, sizeof(*sig->stats));
}

static int cr_import_taskstats(ghost_t *ghost, struct signal_struct *sig)
{
	struct taskstats *stats;
	int err = -ENOMEM;

	stats = kmem_cache_alloc(taskstats_cache, GFP_KERNEL);
	if (!stats)
		goto out;

	err = ghost_read(ghost, stats, sizeof(*stats));
	if (!err)
		sig->stats = stats;
	else
		kmem_cache_free(taskstats_cache, stats);

out:
	return err;
}
#endif

static int cr_export_later_signal_struct(struct epm_action *action,
					 ghost_t *ghost,
					 struct task_struct *task)
{
	int r;
	long key;

	BUG_ON(action->type != EPM_CHECKPOINT);
	BUG_ON(action->checkpoint.shared != CR_SAVE_LATER);

	key = (long)(task->signal->krg_objid);

	r = ghost_write(ghost, &key, sizeof(long));
	if (r)
		goto err;

	r = add_to_shared_objects_list(&task->application->shared_objects,
				       SIGNAL_STRUCT, key, 1 /*is_local*/,
				       task, NULL);

	if (r == -ENOKEY) /* the signal_struct was already in the list */
		r = 0;
err:
	return r;
}

int export_signal_struct(struct epm_action *action,
			 ghost_t *ghost, struct task_struct *tsk)
{
	unsigned long krg_objid = tsk->signal->krg_objid;
	int r;

	if (action->type == EPM_CHECKPOINT &&
	    action->checkpoint.shared == CR_SAVE_LATER) {
		r = cr_export_later_signal_struct(action, ghost, tsk);
		return r;
	}

	r = ghost_write(ghost, &krg_objid, sizeof(krg_objid));
	if (r)
		goto err_write;

	switch (action->type) {
	case EPM_MIGRATE:
		r = export_sigpending(ghost, tsk, &tsk->signal->shared_pending);
		if (r)
			goto err_write;
		r = export_posix_timers(ghost, tsk);
		break;
	case EPM_CHECKPOINT: {
		struct signal_struct *sig =
			kcb_signal_struct_readlock(krg_objid);
		DEBUG(DBG_GHOST_MNGMT, 4, "Begin - pid:%d\n", tsk->pid);
		r = ghost_write(ghost, sig, sizeof(*sig));
		if (!r)
			r = export_sigpending(ghost,
					      tsk,
					      &tsk->signal->shared_pending);
#ifdef CONFIG_TASKSTATS
		if (!r && sig->stats)
			r = cr_export_taskstats(ghost, sig);
#endif
		if (!r)
			r = export_posix_timers(ghost, tsk);
		kcb_signal_struct_unlock(krg_objid);
		DEBUG(DBG_GHOST_MNGMT, 4, "End - pid:%d\n", tsk->pid);
		break;
	} default:
		break;
	}

err_write:
	return r;
}

static int cr_link_to_signal_struct(struct epm_action *action,
				    ghost_t *ghost,
				    struct task_struct *tsk)
{
	int r;
	long key;
	struct signal_struct *sig;

	r = ghost_read(ghost, &key, sizeof(long));
	if (r)
		goto err;

	sig = get_imported_shared_object(&action->restart.app->shared_objects,
					 SIGNAL_STRUCT, key);

	if (!sig) {
		r = -E_CR_BADDATA;
		goto err;
	}

	tsk->signal = sig;

	kh_signal_struct_writelock(tsk->tgid);
	atomic_inc(&tsk->signal->count);
	atomic_inc(&tsk->signal->live);
	kh_share_signal(tsk);

	sig->pgrp = task_pgrp(tsk)->nr;
	set_signal_session(sig, task_session(tsk)->nr);

	kh_signal_struct_unlock(tsk->tgid);
err:
	return r;
}

int import_signal_struct(struct epm_action *action,
			 ghost_t *ghost, struct task_struct *tsk)
{
	unsigned long krg_objid;
	struct signal_struct *sig;
	int r;

	if (action->type == EPM_CHECKPOINT
	    && action->restart.shared == CR_LINK_ONLY) {
		r = cr_link_to_signal_struct(action, ghost, tsk);
		return r;
	}

	r = ghost_read(ghost, &krg_objid, sizeof(krg_objid));
	if (r)
		goto err_read;

	switch (action->type) {
	case EPM_MIGRATE:
		/* TODO: this will need more locking with distributed threads */
		sig = kcb_signal_struct_writelock(krg_objid);
		BUG_ON(!sig);

		WARN_ON(sig->group_exit_task);
		WARN_ON(!list_empty(&sig->shared_pending.list));
		flush_sigqueue(&sig->shared_pending);
		r = import_sigpending(ghost, tsk, &sig->shared_pending);
		if (r)
			goto out_mig_unlock;

		/* This will need proper tty handling once global control ttys
		 * will exist */
		sig->tty = NULL;

		tsk->signal = sig;

		r = import_posix_timers(ghost, tsk);

out_mig_unlock:
		kcb_signal_struct_unlock(krg_objid);

		break;
	case EPM_REMOTE_CLONE:
		/* The structure will be partly copied when creating the
		 * active process */
		sig = kcb_signal_struct_readlock(krg_objid);
		kcb_signal_struct_unlock(krg_objid);
		BUG_ON(!sig);
		tsk->signal = sig;
		break;

	case EPM_CHECKPOINT: {
		struct signal_struct tmp_sig;

		DEBUG(DBG_GHOST_MNGMT, 4,"Begin - pid:%d\n", tsk->pid);
		sig = cr_malloc_signal_struct(tsk->tgid);

		r = ghost_read(ghost, &tmp_sig, sizeof(tmp_sig));
		if (r)
			goto err_free_signal;

		atomic_set(&sig->count, 1);
		atomic_set(&sig->live, 1);

		sig->group_exit_code = tmp_sig.group_exit_code;
		WARN_ON(tmp_sig.group_exit_task);
		sig->notify_count = tmp_sig.notify_count;
		sig->group_stop_count = tmp_sig.group_stop_count;
		sig->flags = tmp_sig.flags;

		r = import_sigpending(ghost, tsk, &sig->shared_pending);
		if (r)
			goto err_free_signal;

		sig->it_real_incr = tmp_sig.it_real_incr;
		sig->it_prof_expires = tmp_sig.it_prof_expires;
		sig->it_virt_expires = tmp_sig.it_virt_expires;
		sig->it_prof_incr = tmp_sig.it_prof_incr;
		sig->it_virt_incr = tmp_sig.it_virt_incr;

		/* This will need proper tty handling once global control ttys
		 * will exist. */
		/* sig->tty = NULL; */

		sig->pgrp = tmp_sig.pgrp;
		sig->tty_old_pgrp = tmp_sig.tty_old_pgrp;
		sig->__session = tmp_sig.__session;
		sig->leader = tmp_sig.leader;

		sig->utime = tmp_sig.utime;
		sig->stime = tmp_sig.stime;
		sig->cutime = tmp_sig.cutime;
		sig->cstime = tmp_sig.cstime;
		sig->nvcsw = tmp_sig.nvcsw;
		sig->nivcsw = tmp_sig.nivcsw;
		sig->cnvcsw = tmp_sig.cnvcsw;
		sig->cnivcsw = tmp_sig.cnivcsw;
		sig->min_flt = tmp_sig.min_flt;
		sig->maj_flt = tmp_sig.maj_flt;
		sig->cmin_flt = tmp_sig.cmin_flt;
		sig->cmaj_flt = tmp_sig.cmaj_flt;
		sig->sched_time = tmp_sig.sched_time;

		memcpy(sig->rlim, tmp_sig.rlim, sizeof(sig->rlim));
#ifdef CONFIG_BSD_PROCESS_ACCT
		sig->pacct = tmp_sig.pacct;
#endif
#ifdef CONFIG_TASKSTATS
		if (tmp_sig.stats) {
			r = cr_import_taskstats(ghost, sig);
			if (r)
				goto err_free_signal;
		}
#endif

		tsk->signal = sig;

		r = import_posix_timers(ghost, tsk);
		if (r)
			goto err_free_signal;

		kcb_signal_struct_unlock(krg_objid);
		DEBUG(DBG_GHOST_MNGMT, 4,"End - pid:%d\n", tsk->pid);
		break;

err_free_signal:
		cr_free_signal_struct(tsk->tgid);
		goto err_read;

	} default:
		PANIC("Case not supported: %d\n", action->type);
	}

err_read:
	return r;
}

void unimport_signal_struct(struct task_struct *task)
{
	/* TODO: for restart, we must free the created kddm signal_struct
	 * object. */
	unimport_posix_timers(task);
	unimport_sigpending(task, &task->signal->shared_pending);
}

void free_ghost_signal(struct task_struct *tsk)
{
	DEBUG(DBG_G_SIGNAL, 1, "starting...\n");
}

static int cr_export_now_signal_struct(struct epm_action *action,
				       ghost_t *ghost,
				       struct task_struct *task,
				       union export_args *args)
{
	int r;
	r = export_signal_struct(action, ghost, task);
	return r;
}

static int cr_import_now_signal_struct(struct epm_action *action,
				       ghost_t *ghost,
				       struct task_struct *fake,
				       void **returned_data)
{
	int r;
	BUG_ON(*returned_data != NULL);

	r = import_signal_struct(action, ghost, fake);
	if (r)
		goto err;

	*returned_data = fake->signal;
err:
	return r;
}

static int cr_import_complete_signal_struct(struct task_struct *fake,
					    void *_sig)
{
	pid_t signal_id = 0;
	struct signal_struct *sig = _sig;

	signal_id = __kcb_exit_signal(sig->krg_objid);

	atomic_dec(&sig->count);
	atomic_dec(&sig->live);

	if (signal_id)
		kh_signal_struct_unlock(signal_id);

	return 0;
}

static int cr_delete_signal_struct(struct task_struct *fake, void *_sig)
{
	pid_t signal_id = 0;
	struct signal_struct *sig = _sig;

	fake->signal = sig;
	INIT_LIST_HEAD(&fake->cpu_timers[0]);
	INIT_LIST_HEAD(&fake->cpu_timers[1]);
	INIT_LIST_HEAD(&fake->cpu_timers[2]);

	signal_id = __kcb_exit_signal(sig->krg_objid);

	atomic_dec(&sig->count);
	atomic_dec(&sig->live);

	if (signal_id)
		kh_signal_struct_unlock(signal_id);

	posix_cpu_timers_exit_group(fake);

	flush_sigqueue(&sig->shared_pending);
	taskstats_tgid_free(sig);
	__cleanup_signal(sig);

	return 0;
}

struct shared_object_operations cr_shared_signal_struct_ops = {
        .restart_data_size = 0,
        .export_now        = cr_export_now_signal_struct,
	.import_now        = cr_import_now_signal_struct,
	.import_complete   = cr_import_complete_signal_struct,
	.delete            = cr_delete_signal_struct,
};

/* private signals */

int export_private_signals(struct epm_action *action,
			   ghost_t *ghost,
			   struct task_struct *task)
{
	int err = 0;

	switch (action->type) {
	case EPM_MIGRATE:
	case EPM_CHECKPOINT:
		err = export_sigpending(ghost, task, &task->pending);
		break;
	default:
		break;
	}

	return err;
}

int import_private_signals(struct epm_action *action,
			   ghost_t *ghost,
			   struct task_struct *task)
{
	int err = 0;

	switch (action->type) {
	case EPM_MIGRATE:
	case EPM_CHECKPOINT:
		err = import_sigpending(ghost, task, &task->pending);
		break;
	default:
		break;
	}

	return err;
}

void unimport_private_signals(struct task_struct *task)
{
	unimport_sigpending(task, &task->pending);
}
