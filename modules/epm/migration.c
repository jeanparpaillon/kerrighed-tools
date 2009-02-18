/*
 *  Kerrighed/modules/epm/migration.c
 *
 *  Copyright (C) 1999-2006 INRIA, Universite de Rennes 1, EDF
 *  Copyright (C) 2006-2007 Pascal Gallard - Kerlabs, Louis Rilling - Kerlabs
 */

/** Migration interface.
 *  @file migration.c
 *
 *  Implementation of migration functions.
 *
 *  @author Geoffroy Vallee
 */

#include <linux/module.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/file.h>
#include <linux/fs_struct.h>
#include <linux/pid_namespace.h>
#include <kerrighed/kerrighed_signal.h>
#include <kerrighed/sys/types.h>
#include <kerrighed/krgnodemask.h>
#include <kerrighed/krginit.h>
#ifdef CONFIG_KRG_CAP
#include <kerrighed/capabilities.h>
#endif
#ifdef CONFIG_KRG_SCHED
#include <kerrighed/module_hook.h>
#endif
#ifdef CONFIG_KRG_SYSCALL_EXIT_HOOK
#include <kerrighed/syscalls.h>
#endif
#include <asm/pgtable.h>
#include <asm/ptrace.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <kerrighed/sched.h>
#include <kerrighed/task.h>

#include "debug_epm.h"

#define MODULE_NAME "migration"

#include <tools/syscalls.h>
#include <hotplug/hotplug.h>
#include <rpc/rpcid.h>
#include <rpc/rpc.h>
#include <proc/task.h>
#include <proc/children.h>
#include <proc/signal_management.h>
#include <proc/sighand_management.h>
#include <proc/krg_exit.h>
#include <proc/krg_fork.h>
#include <ghost/ghost.h>
#include <mm/mm_struct.h>
#include <epm/ghost_process_management.h>
#include <epm/ghost_process_api.h>
#include <epm/action.h>
#include <epm/epm_internal.h>
#ifdef CONFIG_KRG_SYSCALL_EXIT_HOOK
#include <epm/migration_api.h>
#endif

#ifdef CONFIG_KRG_SCHED
struct module_hook_desc kmh_migration_start;
struct module_hook_desc kmh_migration_end;
struct module_hook_desc kmh_migration_aborted;
EXPORT_SYMBOL(kmh_migration_start);
EXPORT_SYMBOL(kmh_migration_end);
EXPORT_SYMBOL(kmh_migration_aborted);
#endif

#define si_node(info)	(*(kerrighed_node_t *) &(info)._sifields._pad)

clock_t usecs_to_ct(unsigned long usec)
{
	int a, b;
	clock_t ticks;

	DEBUG(DBG_MIGRATION, 2, "usec = %ld\n", usec);
	if (usec > 1000000)
		ticks = usec * HZ / 1000000;
	else {
		a = usec / 1000000;
		b = usec - a * 1000000;
		ticks = a * HZ + b * HZ / 1000000;
	}
	DEBUG(DBG_MIGRATION, 2, "migration time in ticks : %ld\n", ticks);
	return ticks;
}

static int migration_implemented(struct task_struct *task)
{
	int ret = 0;

	if (!task->sighand->krg_objid || !task->signal->krg_objid ||
	    !task->task_obj || !task->children_obj ||
	    (task->real_parent != baby_sitter &&
	     task->real_parent != child_reaper(task) &&
	     !task->parent_children_obj))
		goto out;

	/* Note: currently useless, since CLONE_THREAD implies CLONE_VM, but
	 * will become useful when CLONE_VM will be supported. */
	if (!thread_group_empty(task))
		goto out;

	task_lock(task);

	/* No kernel thread, no task sharing its VM */
	if (!task->mm || (task->flags & PF_BORROWED_MM) ||
	    atomic_read(&task->mm->mm_ltasks) > 1)
		goto out_unlock;

	/* No task sharing its signal handlers */
	/* Note: currently useless since CLONE_SIGHAND implies CLONE_VM, but
	 * will become useful when CLONE_VM will be supported */
	if (atomic_read(&task->sighand->count) > 1)
		goto out_unlock;

	/* No task sharing its file descriptors table */
	if (!task->files || atomic_read(&task->files->count) > 1)
		goto out_unlock;

	/* No task sharing its fs_struct */
	if (!task->fs || atomic_read(&task->fs->count) > 1)
		goto out_unlock;

	ret = 1;
out_unlock:
	task_unlock(task);
out:
	return ret;
}

int __may_migrate(struct task_struct *task, struct caller_creds *creds)
{
	return (pid_alive(task)
		/* check permissions */
		&& permissions_ok(task, creds)
#ifdef CONFIG_KRG_CAP
		/* check capabilities */
		&& can_use_krg_cap(task, CAP_CAN_MIGRATE)
#endif /* CONFIG_KRG_CAP */
		&& !krg_action_pending(task, EPM_MIGRATE)
		/* Implementation limitation */
		&& migration_implemented(task));
}

int may_migrate(struct task_struct *task, struct caller_creds *creds)
{
	int retval;

	read_lock(&tasklist_lock);
	retval = __may_migrate(task, creds);
	read_unlock(&tasklist_lock);

	return retval;
}
EXPORT_SYMBOL(may_migrate);

void migration_aborted(struct task_struct *tsk)
{
#ifdef CONFIG_KRG_SCHED
	module_hook_call(&kmh_migration_aborted, (unsigned long) tsk);
#endif
	krg_action_stop(tsk, EPM_MIGRATE);
}

/* Expects tasklist_lock locked */
int do_migrate_process(struct task_struct *task_to_migrate,
		       kerrighed_node_t destination_node_id)
{
	struct siginfo info;
	int retval;

	DEBUG(DBG_MIGRATION, 2,
	      "Migration initialization"
	      " for process %s (of pid %d) to node %d\n",
	      task_to_migrate->comm, task_to_migrate->pid, destination_node_id);

	if (!krgnode_online(destination_node_id))
		return -ENONET;

	if (destination_node_id == kerrighed_node_id) {
		DEBUG(DBG_MIGRATION, 2,
		      "Loopback migration... Nothing to do !\n");
		return 0;
	}

	if (!migration_implemented(task_to_migrate)) {
		printk("do_migrate_process: trying to migrate a thread"
		       " of a multi-threaded process!\n Aborting...\n");
		return -ENOSYS;
	}

	retval = krg_action_start(task_to_migrate, EPM_MIGRATE);
	if (retval)
		return retval;

#ifdef CONFIG_KRG_SCHED
	module_hook_call(&kmh_migration_start, (unsigned long) task_to_migrate);
#endif

	info.si_errno = 0;
	info.si_pid = 0;
	info.si_uid = 0;
	si_node(info) = destination_node_id;

	retval = send_kerrighed_signal(KRG_SIG_MIGRATE, &info, task_to_migrate);
	if (retval)
		migration_aborted(task_to_migrate);

	return retval;
}


/** This function send all the informations about a processus for a migration
 * @author           Geoffroy Vallee
 *
 * @param tsk		Pointer on task #ifdef TIME_MESURE to send
 * @param regs		Pointer on registers of task to send
 * @param remote_node	Target node
 *
 * @return          pid of remote process on success, -1 on fail
 */
pid_t send_task(struct task_struct *tsk,
		struct pt_regs *task_regs,
		kerrighed_node_t remote_node,
		struct epm_action *action)
{
	pid_t pid_remote_task = -1;
	struct rpc_desc *desc;
	ghost_t *ghost;
	int err;

	DEBUG(DBG_MIGRATION, 1, "%d -> %d ...\n", tsk->pid, remote_node);

	BUG_ON(!tsk);

	err = -ENOMEM;
	desc = rpc_begin(EPM_SEND_TASK, remote_node);
	if (!desc)
		goto out_err;

	ghost = create_network_ghost(GHOST_WRITE | GHOST_READ, desc);
	if (IS_ERR(ghost)) {
		err = PTR_ERR(ghost);
		goto out_err_cancel;
	}

	err = rpc_pack_type(desc, *action);
	if (err)
		goto out_err_close;

	/* if the process is a thread of a parallel application, we first stop
	 * all the application threads */
/* 	if (tsk->krg_task->aragorn->to_migr == 1 && tsk->krg_task->aragorn->gthread_infos != NULL) */
/* 		stop_all_the_threads (tsk->pid); */

	err = export_process(action, ghost, tsk, task_regs);
	if (err)
		goto out_err_close;

	DEBUG(DBG_MIGRATION, 1, "Process sent, waiting for its local pid\n");

	err = rpc_unpack_type(desc, pid_remote_task);
	if (err) {
		if (err == RPC_EPIPE)
			err = -EPIPE;
		BUG_ON(err > 0);
		goto out_err_close;
	}

	ghost_close(ghost);

	rpc_end(desc, 0);

	DEBUG(DBG_MIGRATION, 1, "Remote_pid = %d\n", pid_remote_task);
	DEBUG(DBG_MIGRATION, 1,
	      "*********************************************************\n");
	DEBUG(DBG_MIGRATION, 1,
	      "*********************************************************\n");

	DEBUG(DBG_MIGRATION, 1, "migration succeed\n");

	return pid_remote_task;

 out_err_close:
	ghost_close(ghost);
 out_err_cancel:
	rpc_cancel(desc);
	rpc_end(desc, 0);
 out_err:
	return err;
}


static int do_task_migrate(struct task_struct *tsk, struct pt_regs *regs,
			   kerrighed_node_t target)
{
	struct epm_action migration;
	pid_t remote_pid;

	BUG_ON(tsk == NULL);
	BUG_ON(regs == NULL);

	/*
	 * Check again that we actually are able to migrate tsk
	 * For instance fork() may have created a thread right after the
	 * migration request.
	 */
#ifdef CONFIG_KRG_CAP
	if (!can_use_krg_cap(tsk, CAP_CAN_MIGRATE))
		return -ENOSYS;
#endif
	if (!migration_implemented(tsk))
		return -ENOSYS;

	migration.type = EPM_MIGRATE;
	migration.migrate.pid = tsk->pid;
	migration.migrate.target = target;

	DEBUG(DBG_MIGRATION, 1, "%d -> %d ...\n", tsk->pid, target);

	kcb_unset_pid_location(tsk->pid);

	kcb_task_writelock(tsk->pid);
	DEBUG(DBG_MIGRATION, 3,
	      "moving the migrated task from its local relations\n");
	leave_all_relatives(tsk);
	DEBUG(DBG_MIGRATION, 3, "process is now alone and can be migrated\n");
	kcb_task_unlock(tsk->pid);

	kcb_task_unlink(tsk->task_obj, 1);

	/*
	 * Prevent the migrated task from removing the sighand_struct and
	 * signal_struct copies before migration cleanup ends
	 */
	kcb_sighand_struct_pin(tsk->sighand);
	kcb_signal_struct_pin(tsk->signal);

	remote_pid = send_task(tsk, regs, target, &migration);

	if (remote_pid < 0) {
		struct task_kddm_object *obj;

		kcb_signal_struct_writelock(tsk->tgid);
		kcb_signal_struct_unlock(tsk->tgid);
		kcb_signal_struct_unpin(tsk->signal);

		kcb_sighand_struct_writelock(tsk->sighand->krg_objid);
		kcb_sighand_struct_unlock(tsk->sighand->krg_objid);
		kcb_sighand_struct_unpin(tsk->sighand);

		obj = kcb_task_writelock(tsk->pid);
		BUG_ON(!obj);
		write_lock_irq(&tasklist_lock);
		obj->task = tsk;
		tsk->task_obj = obj;
		write_unlock_irq(&tasklist_lock);
		kcb_task_unlock(tsk->pid);

		join_local_relatives(tsk);

		kcb_set_pid_location(tsk->pid, kerrighed_node_id);
	} else {
		BUG_ON(remote_pid != tsk->pid);
		/* Do not notify a task having done vfork() */
		cleanup_vfork_done(tsk);
	}

	/* Need to update process group map here (when map of node will be
	 * available) */

	DEBUG(DBG_MIGRATION, 1, "Done\n");
	return remote_pid > 0 ? 0 : remote_pid;
}


static void do_post_reception(struct task_struct *task,
			      struct epm_action *action)
{
	DEBUG(DBG_MIGRATION, 1,
	      "*********************************************************\n");
	DEBUG(DBG_MIGRATION, 1,
	      "*********************************************************\n");

	switch (action->type) {
	case EPM_MIGRATE:
#ifdef CONFIG_KRG_SCHED
		module_hook_call(&kmh_migration_end, (unsigned long) task);
#endif
		krg_action_stop(task, EPM_MIGRATE);
		break;

	case EPM_REMOTE_CLONE:
		krg_action_stop(task, EPM_REMOTE_CLONE);
		DEBUG(DBG_MIGRATION, 3, "done\n");
		break;

	default:
		PANIC("uncatched case\n");
	}

	DEBUG(DBG_MIGRATION, 3, "Done\n");

	BUG_ON(task->task_obj->task != task);
	wake_up_new_task(task, CLONE_VM);
}


/** This function receive the process informations for the migration.
 *  The ghost process is created with these informations.
 *  @author    Geoffroy Vallee
 *
 *  @return    Pointer on the running task (created with the ghost task)
 */
static struct task_struct *recv_task(struct rpc_desc *desc,
				     struct epm_action *action)
{
	struct task_struct *new_tsk;
/* 	unsigned long migration_time = 0; */
	ghost_t *ghost;
	pid_t pid;
	int err;

	DEBUG(DBG_MIGRATION, 1, "start\n");

	ghost = create_network_ghost(GHOST_READ | GHOST_WRITE, desc);
	if (IS_ERR(ghost))
		goto err_ghost;

	new_tsk = import_process(action, ghost);
	if (IS_ERR(new_tsk))
		goto err_close;
	DEBUG(DBG_MIGRATION, 1, "%d imported\n", new_tsk->pid);

	pid = new_tsk->pid;
	err = rpc_pack_type(desc, pid);
	if (err)
		goto err_close;

/* 	if (action->type == EPM_MIGRATE) */
/* 		new_tsk->stime += usecs_to_ct(migration_time); */

	ghost_close(ghost);

	return new_tsk;

 err_close:
	ghost_close(ghost);
 err_ghost:
	/* TODO: send a custom error code */
	rpc_cancel(desc);
	return NULL;
}

static void kcb_task_migrate(int sig, struct siginfo *info,
			     struct pt_regs *regs)
{
	struct task_struct *tsk = current;
	int r = 0;

	DEBUG(DBG_MIGRATION, 1, "%s(%d) -> %d\n",
	      tsk->comm, tsk->pid, si_node(*info));

	r = do_task_migrate(tsk, regs, si_node(*info));

	DEBUG(DBG_MIGRATION, 1, "%s(%d): r=%d\n",
	      tsk->comm, tsk->pid, r);
	if (!r) {
#ifdef CONFIG_KRG_SCHED
		module_hook_call(&kmh_migration_end, 0);
#endif
		do_exit_wo_notify(0); /* Won't return */
	}

	/* Migration failed */
	migration_aborted(tsk);
}


/*****************************************************************************/
/*                                                                           */
/*                              REQUEST HANDLERS                             */
/*                                                                           */
/*****************************************************************************/

/** Process migration handler.
 *  @author Renaud Lottiaux, Geoffroy Vallée
 *
 *  @param sender   Request sender.
 *  @param msg      Received message.
 */
void handle_task(struct rpc_desc *desc, void *_msg, size_t size)
{
	struct epm_action *action = _msg;
	struct task_struct *new_task = NULL;

	DEBUG(DBG_MIGRATION, 1, "start\n");
	new_task = recv_task(desc, action);
	if (new_task) {
		DEBUG(DBG_MIGRATION, 1, "0x%p:%d\n",
		      new_task, new_task->pid);
		do_post_reception(new_task, action);
	}

	DEBUG(DBG_MIGRATION, 1, "%s\n", new_task ? "success" : "failed");
}

#ifdef CONFIG_KRG_SYSCALL_EXIT_HOOK
static void kcb_syscall_exit(long syscall_nr)
{
	struct caller_creds creds = {
		.caller_uid = 0,
		.caller_euid = 0
	};
	migrate_linux_threads(current->pid, MIGR_LOCAL_PROCESS,
			      next_krgnode_in_ring(kerrighed_node_id,
						   krgnode_online_map),
			      &creds);
}
#endif

void register_migration_hooks(void)
{
	hook_register(&kh_krg_handler[KRG_SIG_MIGRATE], kcb_task_migrate);
#ifdef CONFIG_KRG_SYSCALL_EXIT_HOOK
	hook_register(&kh_syscall_exit, kcb_syscall_exit);
#endif
}


/* Migration Server Initialisation */

int epm_migration_start(void)
{
	DEBUG(DBG_MIGRATION, 1, "Migration server init\n");

	/***  Init the migration serveur  ***/
	rpc_register_void(EPM_SEND_TASK, handle_task, 0);

	DEBUG(DBG_MIGRATION, 1, "Done\n");

	return 0;
}


/* Migration Server Finalization */

void epm_migration_exit(void)
{
}
