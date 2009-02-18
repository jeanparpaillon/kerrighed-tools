/*
 *  Kerrighed/modules/epm/ghost_process_management.c
 *
 *  Copyright (C) 1999-2006 INRIA, Universite de Rennes 1, EDF
 *  Copyright (C) 2006-2007 Pascal Gallard - Kerlabs, Louis Rilling - Kerlabs
 */

#include <linux/types.h>
#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/pid.h>
#include <linux/pid_namespace.h>
#include <linux/file.h>
#include <linux/thread_info.h>
#include <linux/nsproxy.h>
#include <linux/utsname.h>
#include <linux/list.h>
#include <linux/rcupdate.h>
#include <kerrighed/krginit.h>
#include <kerrighed/sched.h>
#include <kerrighed/krgsyms.h>
#include <kerrighed/children.h>
#include <kerrighed/task.h>
#include <kerrighed/pid.h>
#include <kerrighed/ghost.h>
#include <kerrighed/ctnr_headers.h>
#include <kerrighed/signal.h>

#include <asm/thread_info.h>

#define MODULE_NAME "ghost_process_mgt"

#include "debug_epm.h"

#include <proc/sighand_management.h>
#include <proc/task.h>
#include <proc/children.h>
#include <proc/krg_fork.h>
#include <proc/pid_mobility.h>
#include <epm/migration.h>
#include <ctnr/mobility.h>
#include <fs/mobility.h>
#include <mm/mobility.h>
#include <ipc/mobility.h>
#include <proc/distant_syscalls.h>
#ifdef CONFIG_KRG_SCHED_CONFIG
#include <scheduler/core/krg_sched_info.h>
#include <scheduler/core/process_set_mobility.h>
#endif
#include "action.h"
#include "application/application.h"
#include "application/app_shared.h"
#include "g_signal.h"
#include "ghost_process_management.h"
#include "ghost_process_api.h"
#include "ghost_arch.h"

#include <asm/cpu_register.h>

#ifdef CONFIG_KRG_DEBUG
#define MAGIC_BASE 424242
#endif



/*****************************************************************************/
/*                                                                           */
/*                              EXPORT FUNCTIONS                             */
/*                                                                           */
/*****************************************************************************/

#ifdef CONFIG_KRG_SCHED_LEGACY
static int export_krg_sched_info(struct epm_action *action,
				 ghost_t *ghost, struct task_struct *task)
{
	/* Nothing to do yet */
	return 0;
}
#endif


int export_exec_domain(struct epm_action *action,
		       ghost_t *ghost, struct task_struct *tsk)
{
	if (tsk->thread_info->exec_domain != &default_exec_domain)
		PANIC("Cannot ghost a process using an exec_domain != "
		      "default_exec_domain\n");

	return 0;
}


static int export_cpu_timers(struct epm_action *action,
			     ghost_t *ghost, struct task_struct *tsk)
{
	/* Not implemented yet */
	if (action->type != EPM_REMOTE_CLONE) {
		WARN_ON(!list_empty(&tsk->cpu_timers[0]));
		WARN_ON(!list_empty(&tsk->cpu_timers[1]));
		WARN_ON(!list_empty(&tsk->cpu_timers[2]));
	}
	return 0;
}


int export_restart_block(struct epm_action *action,
			 ghost_t *ghost, struct task_struct *tsk)
{
	enum krgsyms_val fn_id;
	int r;

	fn_id = krgsyms_export(tsk->thread_info->restart_block.fn);
	r = ghost_write(ghost, &fn_id, sizeof(fn_id));
	if (r)
		goto err_write;
	r = ghost_write(ghost,
			&tsk->thread_info->restart_block,
			sizeof(tsk->thread_info->restart_block));

err_write:
	return r;
}


static int export_keys(struct epm_action *action,
		       ghost_t *block, struct task_struct *tsk)
{
#ifdef CONFIG_KEYS
	if (tsk->request_key_auth || tsk->thread_keyring) {
		printk("At this time,"
		       " cannot migrate keys informations... gloups!\n");
		BUG();
	}
#endif

	return 0;
}


static int export_user_struct(struct epm_action *action,
			      ghost_t *ghost, struct task_struct *tsk)
{
	/* nothing to do until we manage user accounting accross the cluster */
	return 0;
}


static int export_sched_info(struct epm_action *action,
			     ghost_t *ghost, struct task_struct *tsk)
{
	/* Nothing to do... */
	return 0;
}


static int export_group_info(struct epm_action *action,
			     ghost_t *ghost, struct task_struct *tsk)
{
	/* Nothing done right now. Can be managed using a container. */
	return 0;
}


static int export_group_leader(struct epm_action *action,
			       ghost_t *ghost, struct task_struct *tsk)
{
	return 0;
}


static int export_pids(struct epm_action *action,
		       ghost_t *ghost, struct task_struct *tsk)
{
	enum pid_type type;
	int retval = 0; /* Prevent gcc from warning */

#ifdef CONFIG_KRG_SCHED_CONFIG
	retval = export_process_set_links_start(action, ghost, tsk);
	if (retval)
		goto out;
#endif /* CONFIG_KRG_SCHED_CONFIG */

	for (type = 0; type < PIDTYPE_MAX; type++)
		if (type != PIDTYPE_PID
		    || action->type != EPM_REMOTE_CLONE) {
			retval = export_pid(action, ghost, &tsk->pids[type]);
			if (retval)
				goto out;
#ifdef CONFIG_KRG_SCHED_CONFIG
			retval = export_process_set_links(action, ghost,
							  tsk->pids[type].pid,
							  type);
			if (retval)
				goto out;
#endif /* CONFIG_KRG_SCHED_CONFIG */
		}

#ifdef CONFIG_KRG_SCHED_CONFIG
	retval = export_process_set_links_end(action, ghost, tsk);
#endif /* CONFIG_KRG_SCHED_CONFIG */

 out:
	return retval;
}


static int export_array(struct epm_action *action,
			ghost_t *ghost, struct task_struct *tsk)
{
	/* Nothing to do... */
	return 0;
}


static int export_uts_namespace(struct epm_action *action,
				ghost_t *ghost, struct task_struct *tsk)
{
	if (tsk->nsproxy->uts_ns != &init_uts_ns) {
		/* UTS namespace sharing is not implemented yet */
		PANIC("Cannot export processes"
		      " using a non default UTS namespace!\n");
		return -EINVAL;
	}

	return 0;
}


static int export_nsproxy(struct epm_action *action,
			  ghost_t *ghost, struct task_struct *tsk)
{
	int retval;

	retval = export_uts_namespace(action, ghost, tsk);
	if (retval)
		goto out;
	retval = export_ipc_namespace(action, ghost, tsk);
	if (retval)
		goto err_ipc;
	retval = export_mnt_namespace(action, ghost, tsk);
	if (retval)
		goto err_mnt;
	retval = export_pid_namespace(action, ghost, tsk);
	if (retval)
		goto err_pid;

 out:
	return retval;

 err_pid:
 err_mnt:
 err_ipc:
	goto out;
}


static int export_krg_structs(struct epm_action *action,
			      ghost_t *ghost, struct task_struct *tsk)
{
	DEBUG(DBG_GHOST_MNGMT, 3, "ghost of the process %s created\n",
	      tsk->comm);
	return 0;
}


static int cr_export_later_sighand_struct(struct epm_action *action,
					  ghost_t *ghost,
					  struct task_struct *task)
{
	int r;
	long key;

	BUG_ON(action->type != EPM_CHECKPOINT);
	BUG_ON(action->checkpoint.shared != CR_SAVE_LATER);

	key = (long)(task->sighand);

	r = ghost_write(ghost, &key, sizeof(long));
	if (r)
		goto err;

	/* WARNING, currently we do not really support sighand shared by
	   several nodes */
	r = add_to_shared_objects_list(&task->application->shared_objects,
				       SIGHAND_STRUCT, key, 1 /*is_local*/,
				       task, NULL);

	if (r == -ENOKEY) /* the sighand_struct was already in the list */
		r = 0;
err:
	return r;
}


static int export_sighand_struct(struct epm_action *action,
				 ghost_t *ghost, struct task_struct *tsk)
{
	int r;
	DEBUG(DBG_GHOST_MNGMT, 3, "%lu\n", tsk->sighand->krg_objid);

 	if (action->type == EPM_CHECKPOINT &&
 	    action->checkpoint.shared == CR_SAVE_LATER) {
 		int r;
 		r = cr_export_later_sighand_struct(action, ghost, tsk);
 		return r;
 	}

	r = ghost_write(ghost, &tsk->sighand->krg_objid,
			sizeof(tsk->sighand->krg_objid));
	if (r)
		goto err_write;

	if (action->type == EPM_CHECKPOINT) {
		DEBUG(DBG_GHOST_MNGMT, 4, "Begin - pid:%d\n", tsk->pid);
		r = ghost_write(ghost, tsk->sighand, sizeof(*tsk->sighand));
		DEBUG(DBG_GHOST_MNGMT, 4, "End - pid:%d\n", tsk->pid);
	}

err_write:
	return r;
}

static int export_children_pids(struct epm_action *action,
				ghost_t *ghost, struct task_struct *tsk)
{
	/* Managed by children kddm object */
	return 0;
}


/** Export a process task struct.
 *  @author  Renaud Lottiaux, Geoffroy Vallee
 *
 *  @param ghost   Ghost where file data should be stored.
 *  @param task    Task to export file data from.
 *  @param l_regs  Registers of the task to export.
 *  @param fs      FS segment of the task to send.
 *  @param gs      GS segment of the task to send.
 *
 *  @return  0 if everything ok.
 *           Negative value otherwise.
 */
int export_task(struct epm_action *action,
		ghost_t *ghost,
		struct task_struct *task,
		struct pt_regs *l_regs)
{
	int r;
#ifdef CONFIG_KRG_DEBUG
	int magic = MAGIC_BASE;
#define DEBUG_WRITE_MAGIC					\
	do {							\
		r = ghost_write(ghost, &magic, sizeof(int));	\
		magic++;					\
		if (r)						\
			goto error;				\
	} while (0)
#else
#define DEBUG_WRITE_MAGIC do {} while (0)
#endif
	int binfmt_id;

	DEBUG(DBG_GHOST_MNGMT, 1, "Start exporting process %s (%d)\n",
	      task->comm, task->pid);
	BUG_ON(task == NULL);

	/* Check against what we cannot manage right now */
	if (task->journal_info != NULL)
		PANIC("Cannot ghost a process using journalling file system\n");

	if (task->notifier != NULL)
		PANIC("Cannot ghost a process using signal blocking\n");

#ifndef CONFIG_KRG_IPC
	if (task->sysvsem.undo_list != NULL)
               PANIC("Cannot ghost a process using semaphore\n");
#endif

	pgrp_is_cluster_wide(task->signal->pgrp);

	/* Export the task struct, and registers */
	prepare_to_export(task);
	r = ghost_write(ghost, task, sizeof(struct task_struct));
	if (r)
		goto error;
#ifndef __arch_um__
	r = ghost_write(ghost, l_regs, sizeof(struct pt_regs));
	if (r)
		goto error;
#endif

	r = export_thread_info(action, ghost, task);
	if (r)
		goto error;

	DEBUG_WRITE_MAGIC;

#ifdef CONFIG_KRG_SCHED
	r = export_krg_sched_info(action, ghost, task);
	if (r)
		goto error;
#endif

	r = export_sched_info(action, ghost, task);
	if (r)
		goto error;
	r = export_cpu_timers(action, ghost, task);
	if (r)
		goto error;
#ifdef CONFIG_KRG_MM
	r = export_mm_struct(action, ghost, task);
	if (r)
		goto error;
#endif

	DEBUG_WRITE_MAGIC;

	binfmt_id = krgsyms_export(task->binfmt);
	r = ghost_write(ghost, &binfmt_id, sizeof(int));
	if (r)
		goto error;

	r = export_vfork_done(action, ghost, task);
	if (r)
		goto error;
	r = export_group_info(action, ghost, task);
	if (r)
		goto error;
	r = export_group_leader(action, ghost, task);
	if (r)
		goto error;
	r = export_pids(action, ghost, task);
	if (r)
		goto error;

	r = export_user_struct(action, ghost, task);
	if (r)
		goto error;

	DEBUG_WRITE_MAGIC;

	r = export_keys(action, ghost, task);
	if (r)
		goto error;

	r = export_array(action, ghost, task);
	if (r)
		goto error;

	r = export_thread_struct(action, ghost, task);
	if (r)
		goto error;

	DEBUG_WRITE_MAGIC;

#ifdef CONFIG_KRG_DVFS
	r = export_fs_struct(action, ghost, task);
	if (r)
		goto error;
	r = export_files_struct(action, ghost, task);
	if (r)
		goto error;
#endif

	r = export_nsproxy(action, ghost, task);
	if (r)
		goto error;

	DEBUG_WRITE_MAGIC;

	r = export_children_pids(action, ghost, task);
	if (r)
		goto error;
	r = export_krg_structs(action, ghost, task);
	if (r)
		goto error;
	r = export_kddm_info_struct(action, ghost, task);
	if (r)
		goto error;

	DEBUG_WRITE_MAGIC;

	DEBUG(DBG_GHOST_MNGMT, 2, "save_signal_struct : starting...\n");
	r = export_private_signals(action, ghost, task);
	if (r)
		goto error;
	r = export_signal_struct(action, ghost, task);
	if (r)
		goto error;
	r = export_sighand_struct(action, ghost, task);
	if (r)
		goto error;

	DEBUG_WRITE_MAGIC;

#ifdef CONFIG_KRG_IPC
	r = export_sysv_sem(action, ghost, task);
	if (r)
		goto error;
#endif

	DEBUG_WRITE_MAGIC;

error:
	DEBUG(DBG_GHOST_MNGMT, 1, "end: r=%d\n", r);
	return r;
}


/*****************************************************************************/
/*                                                                           */
/*                              UNIMPORT FUNCTIONS                           */
/*                                                                           */
/*****************************************************************************/

static void unimport_krg_structs(struct epm_action  *action,
				 struct task_struct *task)
{
	switch (action->type) {
	case EPM_REMOTE_CLONE:
	case EPM_CHECKPOINT:
		kcb___free_task_struct(task);
		break;
	default:
		break;
	}
}

static void unimport_children_pids(struct epm_action *action, struct task_struct *task)
{
	switch (action->type) {
	case EPM_REMOTE_CLONE:
	case EPM_CHECKPOINT:
		kh_children_writelock(task->tgid);
		kh_exit_children(task);
		break;
	default:
		break;
	}
}

#define unimport_sighand_struct(task)


static void unimport_nsproxy(struct task_struct *task)
{
	free_nsproxy(task->nsproxy);
}


#define unimport_array(task)
#define unimport_keys(task)


static void unimport_user_struct(struct task_struct *task)
{
	free_uid(task->user);
}


static void __unimport_pids(struct task_struct *task, enum pid_type max_type)
{
	enum pid_type type;

	for (type = 0; type < max_type; type++)
		unimport_pid(&task->pids[type]);
}

static void unimport_pids(struct task_struct *task)
{
	__unimport_pids(task, PIDTYPE_MAX);
}


#define unimport_group_leader(task)


static void unimport_group_info(struct task_struct *task)
{
	put_group_info(task->group_info);
}

#define unimport_binfmt(task)
#define unimport_cpu_timers(task)
#define unimport_sched_info(task)


#ifdef CONFIG_KRG_SCHED_LEGACY
static void unimport_krg_sched_info(struct task_struct *task)
{
	kh_free_sched_info(task);
}
#endif


void unimport_task(struct epm_action *action,
		   struct task_struct *ghost_task)
{
#ifdef CONFIG_KRG_IPC
	unimport_sysv_sem(ghost_task);
#endif
	unimport_kddm_info_struct(ghost_task);
	unimport_krg_structs(action, ghost_task);
	unimport_children_pids(action, ghost_task);
	unimport_sighand_struct(ghost_task);
	unimport_signal_struct(ghost_task);
	unimport_private_signals(ghost_task);
	unimport_nsproxy(ghost_task);
#ifdef CONFIG_KRG_DVFS
	unimport_files_struct(ghost_task);
	unimport_fs_struct(ghost_task);
#endif
	unimport_thread_struct(ghost_task);
	unimport_array(ghost_task);
	unimport_keys(ghost_task);
	unimport_user_struct(ghost_task);
	unimport_pids(ghost_task);
	unimport_group_leader(ghost_task);
	unimport_group_info(ghost_task);
	unimport_binfmt(ghost_task);
	unimport_mm_struct(ghost_task);
	unimport_cpu_timers(ghost_task);
	unimport_sched_info(ghost_task);
#ifdef CONFIG_KRG_SCHED
	unimport_krg_sched_info(ghost_task);
#endif
	unimport_thread_info(ghost_task);
	free_task_struct(ghost_task);
}


/*****************************************************************************/
/*                                                                           */
/*                              IMPORT FUNCTIONS                             */
/*                                                                           */
/*****************************************************************************/

#ifdef CONFIG_KRG_SCHED_LEGACY
static int import_krg_sched_info(struct epm_action *action,
				 ghost_t *ghost, struct task_struct *task)
{
	return kh_copy_sched_info(task);
}
#endif


struct exec_domain *import_exec_domain(struct epm_action *action,
				       ghost_t *ghost)
{
	return &default_exec_domain;
}


static int import_cpu_timers(struct epm_action *action,
			     ghost_t *ghost, struct task_struct *tsk)
{
	/* Not supported yet */
	INIT_LIST_HEAD(&tsk->cpu_timers[0]);
	INIT_LIST_HEAD(&tsk->cpu_timers[1]);
	INIT_LIST_HEAD(&tsk->cpu_timers[2]);
	return 0;
}


int import_restart_block(struct epm_action *action,
			 ghost_t *ghost, struct restart_block *p)
{
	enum krgsyms_val fn_id;
	int r;

	r = ghost_read(ghost, &fn_id, sizeof(fn_id));
	if (r)
		goto err_read;
	r = ghost_read(ghost, p, sizeof(*p));
	if (r)
		goto err_read;
	p->fn = krgsyms_import(fn_id);

err_read:
	return r;
}


static int import_keys(struct epm_action *action,
		       ghost_t *ghost, struct task_struct *t)
{
#ifdef CONFIG_KEYS
	t->request_key_auth = NULL;
	t->thread_keyring = NULL;
#endif

	return 0;
}


static int import_user_struct(struct epm_action *action,
			      ghost_t *ghost, struct task_struct *tsk)
{
	struct user_struct *user = alloc_uid(tsk->uid);

	tsk->user = user;
	return 0;
}


static int import_sched_info(struct epm_action *action,
			     ghost_t *ghost, struct task_struct *tsk)
{
#ifdef CONFIG_SCHEDSTATS
	tsk->sched_info.cpu_time = 0;
	tsk->sched_info.pcnt = 0;
#endif
	return 0;
}


static int import_group_info(struct epm_action *action,
			     ghost_t *ghost, struct task_struct *tsk)
{
	struct group_info *group_info;

	/* Nothing done right now. Can be managed using a container. */

	group_info = groups_alloc(1);
/* 	get_group_info(group_info); */

	tsk->group_info = group_info;

	return 0;
}


static int import_group_leader(struct epm_action *action,
			       ghost_t *ghost, struct task_struct *tsk)
{
	/* TODO: at this time, some other part of the code assume that
	 * group_leader = tsk. So, when we will update this code, we will
	 * need to update:
	 * fs/proc/array.c -- do_task_stat
	 */
	tsk->group_leader = tsk;
	return 0;
}


static int import_array(struct epm_action *action,
			ghost_t *ghost, struct task_struct *tsk)
{
	tsk->array = NULL;
	return 0;
}


static int import_uts_namespace(struct epm_action *action,
				ghost_t *ghost, struct task_struct *tsk)
{
	get_uts_ns(&init_uts_ns);
	tsk->nsproxy->uts_ns = &init_uts_ns;

	return 0;
}


static void unimport_uts_namespace(struct task_struct *tsk)
{
	put_uts_ns(tsk->nsproxy->uts_ns);
}


static int import_nsproxy(struct epm_action *action,
			  ghost_t *ghost, struct task_struct *tsk)
{
	struct nsproxy *ns;
	int retval = -ENOMEM;

	ns = kmemdup(current->nsproxy, sizeof(struct nsproxy), GFP_KERNEL);
	if (ns)
		atomic_set(&ns->count, 1);
	tsk->nsproxy = ns;
	if (!ns)
		goto out;

	retval = import_uts_namespace(action, ghost, tsk);
	if (retval)
		goto err_uts;
	retval = import_ipc_namespace(action, ghost, tsk);
	if (retval)
		goto err_ipc;
	retval = import_mnt_namespace(action, ghost, tsk);
	if (retval)
		goto err_mnt;
	retval = import_pid_namespace(action, ghost, tsk);
	if (retval)
		goto err_pid;

 out:
	return retval;

 err_pid:
	unimport_mnt_namespace(tsk);
 err_mnt:
	unimport_ipc_namespace(tsk);
 err_ipc:
	unimport_uts_namespace(tsk);
 err_uts:
	kfree(ns);
	goto out;
}


static int import_pids(struct epm_action *action,
		       ghost_t *ghost, struct task_struct *tsk)
{
	enum pid_type type;
	int retval = 0;

#ifdef CONFIG_KRG_SCHED_CONFIG
	retval = import_process_set_links_start(action, ghost, tsk);
	if (retval)
		goto out;
#endif

	for (type = 0; type < PIDTYPE_MAX; type++) {

		DEBUG(DBG_GHOST_MNGMT, 3, "PidType: %u\n", type);

		if (type == PIDTYPE_PID
		    && action->type == EPM_REMOTE_CLONE) {
			struct pid *pid = alloc_pid();
			if (!pid)
				retval = -ENOMEM;
			else {
				tsk->pids[PIDTYPE_PID].pid = pid;
				tsk->pid = pid->nr;
				if (!(action->remote_clone.clone_flags & CLONE_THREAD))
					tsk->tgid = tsk->pid;
			}

		} else {
			retval = import_pid(action, ghost, &tsk->pids[type]);
		}

		if (retval) {
			__unimport_pids(tsk, type);
			break;
		}
#ifdef CONFIG_KRG_SCHED_CONFIG
		if (type != PIDTYPE_PID || action->type != EPM_REMOTE_CLONE) {
			retval = import_process_set_links(
				action, ghost,
				tsk->pids[type].pid, type);
			if (retval) {
				__unimport_pids(tsk, type + 1);
				break;
			}
		}
#endif /* CONFIG_KRG_SCHED_CONFIG */
	}

#ifdef CONFIG_KRG_SCHED_CONFIG
	if (!retval)
		retval = import_process_set_links_end(action, ghost, tsk);
 out:
#endif

	return retval;
}

static int cr_link_to_sighand_struct(struct epm_action *action,
				     ghost_t *ghost,
				     struct task_struct *tsk)
{
	int r;
	long key;
	struct sighand_struct *sig;

	r = ghost_read(ghost, &key, sizeof(long));
	if (r)
		goto err;

	sig = get_imported_shared_object(&action->restart.app->shared_objects,
					 SIGHAND_STRUCT, key);

	if (!sig) {
		r = -E_CR_BADDATA;
		goto err;
	}
	kh_sighand_struct_writelock(sig->krg_objid);

	atomic_inc(&sig->count);
	tsk->sighand = sig;

	kh_share_sighand(tsk);
	kh_sighand_struct_unlock(sig->krg_objid);
err:
	return r;
}


static int import_sighand_struct(struct epm_action *action,
				 ghost_t *ghost, struct task_struct *tsk)
{
	unsigned long krg_objid;
	struct sighand_struct_kddm_object *obj;
	int r;

 	if (action->type == EPM_CHECKPOINT
 	    && action->restart.shared == CR_LINK_ONLY) {
 		r = cr_link_to_sighand_struct(action, ghost, tsk);
 		return r;
 	}

	r = ghost_read(ghost, &krg_objid, sizeof(krg_objid));
	if (r)
		goto err_read;

	DEBUG(DBG_GHOST_MNGMT, 3, "%lu\n", krg_objid);

	switch (action->type) {
	case EPM_MIGRATE:
		tsk->sighand = kcb_sighand_struct_writelock(krg_objid);
		BUG_ON(!tsk->sighand);
		kcb_sighand_struct_unlock(krg_objid);
		break;
	case EPM_REMOTE_CLONE:
		/* The structure will be partly copied when creating the
		 * active process */
		tsk->sighand = kcb_sighand_struct_readlock(krg_objid);
		BUG_ON(!tsk->sighand);
		kcb_sighand_struct_unlock(krg_objid);
		break;
	case EPM_CHECKPOINT:
		DEBUG(DBG_GHOST_MNGMT, 4, "Begin - pid:%d\n", tsk->pid);
		/* No need to take a write lock on task kddm object, since no
		 * other node will grab it until restart is finished, and we
		 * will grab it before the end of restart. */
		tsk->sighand = cr_malloc_sighand_struct();

		krg_objid = tsk->sighand->krg_objid;
		obj = tsk->sighand->kddm_obj;
		r = ghost_read(ghost, tsk->sighand, sizeof(*(tsk->sighand)));
		if (r) {
			cr_free_sighand_struct(krg_objid);
			goto err_read;
		}
		atomic_set(&tsk->sighand->count, 1);
		tsk->sighand->krg_objid = krg_objid;
		tsk->sighand->kddm_obj = obj;

		spin_lock_init(&tsk->sighand->siglock);

		kcb_sighand_struct_unlock(krg_objid);
		DEBUG(DBG_GHOST_MNGMT, 4, "End - pid:%d\n", tsk->pid);
		break;
	default:
		PANIC("Case not supported: %d\n", action->type);
	}

err_read:
	return r;
}


static int import_children_pids(struct epm_action *action,
				ghost_t *ghost, struct task_struct *tsk)
{
	int r = 0;

	switch (action->type) {
	case EPM_MIGRATE:
		tsk->children_obj = kh_children_readlock(tsk->tgid);
		BUG_ON(!tsk->children_obj);
		kh_children_unlock(tsk->tgid);
		break;

	case EPM_REMOTE_CLONE:
	case EPM_CHECKPOINT:
		/* C/R: there are some more things to do if restarted process
		 * had children */
		if (tsk->pid == tsk->tgid) {
			tsk->children_obj = kh_alloc_children(tsk);
		} else {
			tsk->children_obj = kh_children_writelock(tsk->tgid);
			BUG_ON(!tsk->children_obj);
			kcb___share_children(tsk);
			kh_children_unlock(tsk->tgid);
		}
		if (!tsk->children_obj)
			r = -ENOMEM;
		break;
	default:
		break;
	}

	return r;
}


static int import_krg_structs(struct epm_action *action,
			      ghost_t *ghost, struct task_struct *tsk)
{
	/* Inits are only needed to prevent compiler warnings. */
	pid_t parent_pid = 0, real_parent_pid = 0, real_parent_tgid = 0;
	pid_t group_leader_pid = 0;
	struct task_kddm_object *obj;

	/* Initialization of the shared part of the task_struct */

	if (action->type == EPM_REMOTE_CLONE) {
		if (action->remote_clone.clone_flags & CLONE_THREAD) {
			struct task_kddm_object *item;

			item = kh_task_readlock(action->remote_clone.from_pid);
			BUG_ON(!item);
			parent_pid = item->parent;
			real_parent_pid = item->real_parent;
			real_parent_tgid = item->real_parent_tgid;
			BUG_ON(item->group_leader != action->remote_clone.from_tgid);
			kh_task_unlock(action->remote_clone.from_pid);

			group_leader_pid = action->remote_clone.from_tgid;
		} else {
			parent_pid = action->remote_clone.from_pid;
			real_parent_pid = action->remote_clone.from_pid;
			real_parent_tgid = action->remote_clone.from_tgid;
			group_leader_pid = tsk->pid;
		}
	}

	/* Not a simple write lock because with REMOTE_CLONE and CHECKPOINT the
	 * task container object does not exist yet.
	 */
	obj = kcb_task_create_writelock(tsk->pid);
	BUG_ON(!obj);

	switch (action->type) {
	case EPM_REMOTE_CLONE:
		obj->parent = parent_pid;
		obj->real_parent = real_parent_pid;
		obj->real_parent_tgid = real_parent_tgid;
		obj->group_leader = group_leader_pid;
		break;
	case EPM_MIGRATE:
		break;
	case EPM_CHECKPOINT:
		/* Initialization of the (real) parent pid and real parent
		 * tgid in case of restart */
		/* Bringing restarted processes to foreground will need more
		 * work */
		obj->parent = child_reaper(current)->pid;
		obj->real_parent = child_reaper(current)->pid;
		obj->real_parent_tgid = child_reaper(current)->tgid;
		/* obj->group_leader has already been set when creating the
		 * object. */
		break;
	default:
		BUG();
	}

	kcb_task_unlock(tsk->pid);

	DEBUG(DBG_GHOST_MNGMT, 1, "done\n");

	return 0;
}


/** Import a process task struct.
 *  @author  Renaud Lottiaux, Geoffroy Vallee
 *
 *  @param ghost         Ghost where file data should be stored.
 *  @param l_regs        Registers of the task to imported task.
 *
 *  @return  The task struct of the imported process.
 *           Error code otherwise.
 */
struct task_struct *import_task(struct epm_action *action,
				ghost_t *ghost,
				struct pt_regs *l_regs)
{
	struct task_struct *task;
#ifdef CONFIG_KRG_DEBUG
	int magic = MAGIC_BASE;
	int magic_read = 0;
#define DEBUG_READ_MAGIC(n)							\
	do {								\
		retval = ghost_read(ghost, &magic_read, sizeof(int));	\
		if (retval)						\
			goto err_magic_##n;				\
		if (magic_read != magic)				\
			PANIC("Bad magic number in import_task(" #n ")\n"); \
		magic++;						\
	} while (0)
#else
#define DEBUG_READ_MAGIC(n) do {} while (0)
#endif
	int binfmt_id;
	int retval;

	DEBUG(DBG_GHOST_MNGMT, 1, "starting...\n");

	/* Export the task struct, and registers */
	task = alloc_task_struct();
	if (!task) {
		retval = -ENOMEM;
		goto err_alloc_task;
	}

	retval = ghost_read(ghost, task, sizeof(struct task_struct));
	if (retval)
		goto err_task;
#ifndef __arch_um__
	retval = ghost_read(ghost, l_regs, sizeof(struct pt_regs));
	if (retval)
		goto err_regs;
#endif

	/* Init fields. Paranoia to avoid dereferencing a pointer which has not
	 * meaning on this node.
	 */
	atomic_set(&task->usage, 2);
	INIT_LIST_HEAD(&task->run_list);
	task->array = NULL;
	INIT_LIST_HEAD(&task->tasks);
	INIT_LIST_HEAD(&task->ptrace_children);
	INIT_LIST_HEAD(&task->ptrace_list);
	task->real_parent = NULL;
	task->parent = NULL;
	INIT_LIST_HEAD(&task->children);
	INIT_LIST_HEAD(&task->sibling);
	task->group_leader = task;
	INIT_LIST_HEAD(&task->thread_group);
	task->sysvsem.undo_list = NULL;
	task->security = NULL;
	spin_lock_init(&task->alloc_lock);
	spin_lock_init(&task->pi_lock);
	task->journal_info = NULL;
	task->reclaim_state = NULL;
	task->backing_dev_info = NULL;
	task->io_context = NULL;
	task->last_siginfo = NULL;
	task->io_wait = NULL;
	task->splice_pipe = NULL;
#ifdef CONFIG_TASK_DELAY_ACCT
	task->delays = NULL;
#endif
	task->task_obj = NULL;
	rcu_assign_pointer(task->parent_children_obj, NULL);
	task->children_obj = NULL;
	task->application = NULL;

	/* Almost copy paste from fork.c for lock debugging stuff, to avoid
	 * fooling this node with traces from the exporting node */
#ifdef CONFIG_TRACE_IRQFLAGS
	task->irq_events = 0;
	task->hardirqs_enabled = 1;
	task->hardirq_enable_ip = _THIS_IP_;
	task->hardirq_enable_event = 0;
	task->hardirq_disable_ip = 0;
	task->hardirq_disable_event = 0;
	task->softirqs_enabled = 1;
	task->softirq_enable_ip = _THIS_IP_;
	task->softirq_enable_event = 0;
	task->softirq_disable_ip = 0;
	task->softirq_disable_event = 0;
	task->hardirq_context = 0;
	task->softirq_context = 0;
#endif
#ifdef CONFIG_LOCKDEP
	task->lockdep_depth = 0; /* no locks held yet */
	task->curr_chain_key = 0;
	task->lockdep_recursion = 0;
#endif

#ifdef CONFIG_DEBUG_MUTEXES
	task->blocked_on = NULL; /* not blocked yet */
#endif
	/* End of lock debugging stuff */

	/* Now, let's continue the import of the process */

	retval = import_thread_info(action, ghost, task);
	if (retval)
		goto err_thread_info;

	DEBUG_READ_MAGIC(1);

#ifdef CONFIG_KRG_SCHED
	retval = import_krg_sched_info(action, ghost, task);
	if (retval)
		goto err_krg_sched_info;
#endif

	retval = import_sched_info(action, ghost, task);
	if (retval)
		goto err_sched_info;

	retval = import_cpu_timers(action, ghost, task);
	if (retval)
		goto err_cpu_timers;

#ifdef CONFIG_KRG_MM
	retval = import_mm_struct(action, ghost, task);
	if (retval)
		goto err_mm_struct;
#endif

	DEBUG_READ_MAGIC(2);

	retval = ghost_read(ghost, &binfmt_id, sizeof(int));
	if (retval)
		goto err_binfmt;
	task->binfmt = krgsyms_import(binfmt_id);

	retval = import_vfork_done(action, ghost, task);
	if (retval)
		goto err_vfork_done;

	retval = import_group_info(action, ghost, task);
	if (retval)
		goto err_group_info;
	retval = import_group_leader(action, ghost, task);
	if (retval)
		goto err_group_leader;
	retval = import_pids(action, ghost, task);
	if (retval)
		goto err_pids;

	retval = import_user_struct(action, ghost, task);
	if (retval)
		goto err_user_struct;

	DEBUG_READ_MAGIC(3);

	retval = import_keys(action, ghost, task);
	if (retval)
		goto err_keys;
	retval = import_array(action, ghost, task);
	if (retval)
		goto err_array;

	retval = import_thread_struct(action, ghost, task);
	if (retval)
		goto err_thread_struct;

	DEBUG_READ_MAGIC(4);

#ifdef CONFIG_KRG_DVFS
	retval = import_fs_struct(action, ghost, task);
	if (retval)
		goto err_fs_struct;
	retval = import_files_struct(action, ghost, task);
	if (retval)
		goto err_files_struct;
#endif

	retval = import_nsproxy(action, ghost, task);
	if (retval)
		goto err_nsproxy;

	DEBUG_READ_MAGIC(5);

	retval = import_children_pids(action, ghost, task);
	if (retval)
		goto err_children_pids;
	retval = import_krg_structs(action, ghost, task);
	if (retval)
		goto err_krg_structs;
	retval = import_kddm_info_struct(action, ghost, task);
	if (retval)
		goto err_kddm_info_struct;

	DEBUG_READ_MAGIC(6);

	retval = import_private_signals(action, ghost, task);
	if (retval)
		goto err_signals;
	retval = import_signal_struct(action, ghost, task);
	if (retval)
		goto err_signal_struct;

	retval = import_sighand_struct(action, ghost, task);
	if (retval)
		goto err_sighand_struct;

	DEBUG_READ_MAGIC(7);

#ifdef CONFIG_KRG_IPC
	retval = import_sysv_sem(action, ghost, task);
	if (retval)
		goto err_sysv_sem;
#endif

	DEBUG_READ_MAGIC(8);

	DEBUG(DBG_GHOST_MNGMT, 1, "end\n");
	return task;

#ifdef CONFIG_KRG_DEBUG
 err_magic_8:
#ifdef CONFIG_KRG_IPC
	unimport_sysv_sem(task);
#endif
#endif
#ifdef CONFIG_KRG_IPC
 err_sysv_sem:
#endif
#ifdef CONFIG_KRG_DEBUG
 err_magic_7:
#endif
	unimport_sighand_struct(task);
 err_sighand_struct:
	unimport_signal_struct(task);
 err_signal_struct:
	unimport_private_signals(task);
 err_signals:
#ifdef CONFIG_KRG_DEBUG
 err_magic_6:
#endif
	unimport_kddm_info_struct(task);
 err_kddm_info_struct:
	unimport_krg_structs(action, task);
 err_krg_structs:
	unimport_children_pids(action, task);
 err_children_pids:
#ifdef CONFIG_KRG_DEBUG
 err_magic_5:
#endif
	unimport_nsproxy(task);
 err_nsproxy:
#ifdef CONFIG_KRG_DVFS
	unimport_files_struct(task);
 err_files_struct:
	unimport_fs_struct(task);
 err_fs_struct:
#endif
#ifdef CONFIG_KRG_DEBUG
 err_magic_4:
#endif
	unimport_thread_struct(task);
 err_thread_struct:
	unimport_array(task);
 err_array:
	unimport_keys(task);
 err_keys:
#ifdef CONFIG_KRG_DEBUG
 err_magic_3:
#endif
	unimport_user_struct(task);
 err_user_struct:
	unimport_pids(task);
 err_pids:
	unimport_group_leader(task);
 err_group_leader:
	unimport_group_info(task);
 err_group_info:
 	unimport_vfork_done(task);
 err_vfork_done:
	unimport_binfmt(task);
 err_binfmt:
#ifdef CONFIG_KRG_DEBUG
 err_magic_2:
#endif
#ifdef CONFIG_KRG_MM
	unimport_mm_struct(task);
 err_mm_struct:
#endif
	unimport_cpu_timers(task);
 err_cpu_timers:
	unimport_sched_info(task);
 err_sched_info:
#ifdef CONFIG_KRG_SCHED
	unimport_krg_sched_info(task);
 err_krg_sched_info:
#endif
#ifdef CONFIG_KRG_DEBUG
 err_magic_1:
#endif
	unimport_thread_info(task);
 err_thread_info:
/* 	unimport_regs(task); */
 err_regs:
/* 	unimport_task_struct(task); */
 err_task:
	free_task_struct(task);
 err_alloc_task:
	return ERR_PTR(retval);
}


static int cr_export_now_sighand_struct(struct epm_action *action,
					ghost_t *ghost,
					struct task_struct *task,
					union export_args *args)
{
	int r;
	r = export_sighand_struct(action, ghost, task);
	return r;
}


static int cr_import_now_sighand_struct(struct epm_action *action,
					ghost_t *ghost,
					struct task_struct *fake,
					void ** returned_data)
{
	int r;
	BUG_ON(*returned_data != NULL);

	r = import_sighand_struct(action, ghost, fake);
	if (r)
		goto err;

	*returned_data = fake->sighand;
err:
	return r;
}

static int cr_import_complete_sighand_struct(struct task_struct *fake,
					     void *_sig)
{
	unsigned long sighand_id;
	struct sighand_struct *sig = _sig;
	sighand_id = kh_exit_sighand(sig->krg_objid);
	if (sighand_id)
		kh_sighand_struct_unlock(sighand_id);

	BUG_ON(atomic_read(&sig->count) <= 1);
	__cleanup_sighand(sig);

	return 0;
}

static int cr_delete_sighand_struct(struct task_struct *fake, void *_sig)
{
	unsigned long sighand_id;
	struct sighand_struct *sig = _sig;
	sighand_id = kh_exit_sighand(sig->krg_objid);
	if (sighand_id)
		kh_sighand_struct_unlock(sighand_id);

	BUG_ON(atomic_read(&sig->count) != 1);
	__cleanup_sighand(sig);

	return 0;
}

struct shared_object_operations cr_shared_sighand_struct_ops = {
        .restart_data_size = 0,
        .export_now        = cr_export_now_sighand_struct,
	.import_now        = cr_import_now_sighand_struct,
	.import_complete   = cr_import_complete_sighand_struct,
	.delete            = cr_delete_sighand_struct,
};

/*****************************************************************************/
/*                                                                           */
/*                               TOOLS FUNCTIONS                             */
/*                                                                           */
/*****************************************************************************/

static void free_ghost_task(struct task_struct *task)
{
	DEBUG(DBG_G_TASK, 1, "starting...\n");
	BUG_ON(!task);
	remove_parent(task);
	free_task_struct(task);
	DEBUG(DBG_G_TASK, 1, "done\n");
}


void free_ghost_process(struct task_struct *ghost)
{
	DEBUG(DBG_GHOST_MNGMT, 1, "starting...\n");

	free_ghost_signal(ghost);
	DEBUG(DBG_GHOST_MNGMT, 2, "free_ghost_signal done\n");

	free_ghost_mm(ghost);
	DEBUG(DBG_GHOST_MNGMT, 2, "free_ghost_mm done\n");

#ifdef CONFIG_KRG_DVFS
	free_ghost_files(ghost);
	DEBUG(DBG_GHOST_MNGMT, 2, "free_ghost_files done\n");
#endif

	free_uid(ghost->user);
	put_group_info(ghost->group_info);

	put_nsproxy(ghost->nsproxy);
	kmem_cache_free(kddm_info_cachep, ghost->kddm_info);

	free_ghost_thread_info(ghost);
	DEBUG(DBG_GHOST_MNGMT, 2, "free_ghost_thread_info done\n");

	free_ghost_task(ghost);
	DEBUG(DBG_GHOST_MNGMT, 2, "free_ghost_task done\n");

	DEBUG(DBG_GHOST_MNGMT, 1, "done\n");
}


static int register_pids(struct task_struct *task, struct epm_action *action)
{
	enum pid_type type;

	for (type = 0; type < PIDTYPE_MAX; type++)
		if ((type != PIDTYPE_PID || action->type != EPM_REMOTE_CLONE)
		    && task->pids[type].pid->kddm_obj)
			kh_end_get_pid(task->pids[type].pid);

	return 0;
}


/** This function create the new process from the ghost process
 * @author Geoffroy Vallee, Louis Rilling
 *
 * @param tskRecv               Pointer on the ghost process
 * @param regs			Pointer on the registers of the ghost process
 * @param action		Migration, checkpoint, creation, etc.
 *
 * @return                      Pointer on the running task
 *                              (created with the ghost process)
 */
struct task_struct *create_new_process_from_ghost(struct task_struct *tskRecv,
						  struct pt_regs *l_regs,
						  struct epm_action *action)
{
	int new_pid = -1;
	struct pid *pid;
	struct task_struct *newTsk;
	struct task_kddm_object *obj;
	unsigned long flags;
	unsigned long stack_start;
	unsigned long stack_size;
	int *parent_tidptr;
	int *child_tidptr;
	struct children_kddm_object *parent_children_obj;
	pid_t real_parent_tgid;
	int retval;

	DEBUG(DBG_GHOST_MNGMT, 1, "starting...\n");

	BUG_ON(!l_regs || !tskRecv);

	DEBUG(DBG_GHOST_MNGMT, 3, "Ghost process has %d users of its mm\n",
	      atomic_read(&tskRecv->mm->mm_users));
	DEBUG(DBG_GHOST_MNGMT, 3, "Ghost process has %d references to its mm\n",
	      atomic_read(&tskRecv->mm->mm_count));

	/* The active process must be considered as remote until all links
	 * with parent and children are restored atomically */
	tskRecv->parent = tskRecv->real_parent = baby_sitter;

	/* Re-attach to the children kddm object of the parent. */
	if (action->type == EPM_REMOTE_CLONE) {
		real_parent_tgid = action->remote_clone.from_tgid;
		/* We need writelock to declare the new child later. */
		parent_children_obj = kh_children_writelock(real_parent_tgid);
		BUG_ON(!parent_children_obj);
		kh_children_get(parent_children_obj);
		rcu_assign_pointer(tskRecv->parent_children_obj,
				   parent_children_obj);
	} else {
		pid_t parent, real_parent;

		/* We must not call kh_parent_children_readlock since we are
		 * restoring here the data needed for this function to work. */
		obj = kh_task_readlock(tskRecv->pid);
		real_parent_tgid = obj->real_parent_tgid;
		kh_task_unlock(tskRecv->pid);

		parent_children_obj =
			kh_children_readlock(real_parent_tgid);
		if (!kh_get_parent(parent_children_obj, tskRecv->pid,
				   &parent, &real_parent)) {
			kh_children_get(parent_children_obj);
			rcu_assign_pointer(tskRecv->parent_children_obj,
					   parent_children_obj);
		}
		kh_children_unlock(real_parent_tgid);
	}

	flags = (tskRecv->exit_signal & CSIGNAL) | CLONE_VM | CLONE_THREAD
	    | CLONE_SIGHAND;
	stack_start = CPUREG_SP(l_regs);
	/* Will BUG as soon as used in copy_thread (e.g. ia64, but not i386 and
	 * x86_64) */
	stack_size = 0;
	parent_tidptr = NULL;
	child_tidptr = NULL;

	if (action->type == EPM_REMOTE_CLONE) {
		/* Adjust do_fork parameters */

		/* Do not pollute exit signal of the child with bits from
		 * parent's exit_signal */
		flags &= ~CSIGNAL;
		flags = flags | action->remote_clone.clone_flags;
		DEBUG(DBG_GHOST_MNGMT, 3, "flags=%lx child_tidptr=%p\n",
		      flags, child_tidptr);
		stack_start = action->remote_clone.stack_start;
		stack_size = action->remote_clone.stack_size;
		parent_tidptr = action->remote_clone.parent_tidptr;
		child_tidptr = action->remote_clone.child_tidptr;
	}

	rcu_read_lock();
	pid = find_pid(tskRecv->pid);
	/* No need to increment the ref count of pid, since it cannot be freed
	 * until we call kh_end_get_pid (migration), or until it is attached for
	 * the first time (remote creation). */
	rcu_read_unlock();
	BUG_ON(!pid);

	obj = kcb_task_writelock(tskRecv->pid);

	krg_current = tskRecv;
	new_pid = __do_fork(flags, stack_start, l_regs, stack_size,
			    parent_tidptr, child_tidptr, pid);
	krg_current = NULL;

	if (new_pid < 0) {
		DEBUG(DBG_GHOST_MNGMT, 1, "_do_fork failed: %d\n", new_pid);

		kh_task_unlock(tskRecv->pid);

		if (action->type == EPM_REMOTE_CLONE)
			kh_children_unlock(real_parent_tgid);
		kh_children_put(tskRecv->parent_children_obj);

		return ERR_PTR(new_pid);
	}

	rcu_read_lock();
	newTsk = find_task_by_pid(new_pid);
	/* No need to take a ref on newTsk since nobody can free it until we
	 * schedule it for the first time. */
	rcu_read_unlock();
	if (!newTsk) {
		printk("Could not find newly created task %d\n", new_pid);
		BUG();
	}
	BUG_ON(newTsk->task_obj);
	BUG_ON(obj->task);
	write_lock_irq(&tasklist_lock);
	newTsk->task_obj = obj;
	obj->task = newTsk;
	write_unlock_irq(&tasklist_lock);
	BUG_ON(newTsk->parent_children_obj != tskRecv->parent_children_obj);
	BUG_ON(!newTsk->children_obj);

	BUG_ON(newTsk->exit_signal != (flags & CSIGNAL));
 	BUG_ON(action->type == EPM_MIGRATE &&
	       newTsk->exit_signal != tskRecv->exit_signal);

	if (action->type == EPM_CHECKPOINT)
		newTsk->exit_signal = tskRecv->exit_signal;

	/* TODO: Need a special work for thread process */
	BUG_ON(newTsk->group_leader != newTsk);
	BUG_ON(newTsk->task_obj->group_leader != newTsk->pid);

	if (action->type == EPM_REMOTE_CLONE) {
		retval = kh_new_child(parent_children_obj,
				      action->remote_clone.from_pid,
				      newTsk->pid, newTsk->tgid,
				      process_group(newTsk),
				      process_session(newTsk),
				      newTsk->exit_signal);

		kh_children_unlock(real_parent_tgid);
		if (retval)
			PANIC("Remote child %d of %d created"
			      " but could not be registered!",
			      newTsk->pid,
			      action->remote_clone.from_pid);
	}

	if (action->type == EPM_MIGRATE || action->type == EPM_CHECKPOINT)
		newTsk->did_exec = tskRecv->did_exec;

	kcb_task_unlock(tskRecv->pid);

	DEBUG(DBG_GHOST_MNGMT, 3, "active task struct created\n");

	retval = register_pids(newTsk, action);
	BUG_ON(retval);
	pgrp_is_cluster_wide(process_group(newTsk));

	DEBUG(DBG_GHOST_MNGMT, 3, "pids registered\n");

	if (action->type == EPM_MIGRATE
	    || action->type == EPM_CHECKPOINT) {
		/* signals should be copied from the ghost, as do_fork does not
		 * clone the signal queue */
		if (!sigisemptyset(&tskRecv->pending.signal)
		    || !list_empty(&tskRecv->pending.list)) {
			unsigned long flags;

			DEBUG(DBG_GHOST_MNGMT, 2, "pending with something\n");
			if (!lock_task_sighand(newTsk, &flags))
				BUG();
			list_splice(&tskRecv->pending.list,
				    &newTsk->pending.list);
			sigorsets(&newTsk->pending.signal,
				  &newTsk->pending.signal,
				  &tskRecv->pending.signal);
			unlock_task_sighand(newTsk, &flags);

			init_sigpending(&tskRecv->pending);
		}
		/* Always set TIF_SIGPENDING, since migration/checkpoint
		 * interrupted the task as an (ignored) signal. This way
		 * interrupted syscalls are transparently restarted. */
		set_tsk_thread_flag(newTsk, TIF_SIGPENDING);
	}
	if (newTsk->pid == newTsk->tgid)
		newTsk->signal->tsk = newTsk;
	DEBUG(DBG_GHOST_MNGMT, 3, "Signal fix done\n");

	newTsk->files->next_fd = tskRecv->files->next_fd;

	DEBUG(DBG_GHOST_MNGMT, 3, "next_fd fix done\n");

	if (action->type == EPM_MIGRATE
	    || action->type == EPM_CHECKPOINT) {
		/* Remember process times until now (cleared by do_fork) */
		newTsk->utime = tskRecv->utime;
		/* stime will be updated later to account for migration time */
		newTsk->stime = tskRecv->stime;

		/* Restore flags changed by _do_fork */
		newTsk->flags = tskRecv->flags;

		DEBUG(DBG_GHOST_MNGMT, 3, "times and flags fixed\n");
	}

	/* Atomically restore links with local relatives and allow relatives
	 * to consider newTsk as local.
	 * Until now, newTsk is linked to baby sitter and not linked to any
	 * child. */
	join_local_relatives(newTsk);
	DEBUG(DBG_GHOST_MNGMT, 2, "process links created\n");

#ifdef CONFIG_KRG_SCHED_CONFIG
	post_import_krg_sched_info(newTsk);
#endif

	/* Now the process can be made world-wide visible */
	kcb_set_pid_location(newTsk->pid, kerrighed_node_id);

	DEBUG(DBG_GHOST_MNGMT, 3, "fpu_counter=%d\n", newTsk->fpu_counter);

	DEBUG(DBG_GHOST_MNGMT, 1, "new process pid: %d\n", new_pid);
	DEBUG(DBG_GHOST_MNGMT, 1, "done...\n");

	return newTsk;
}
