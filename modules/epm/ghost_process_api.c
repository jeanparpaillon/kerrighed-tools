/*
 *  Kerrighed/modules/epm/ghost_process_api.c
 *
 *  Copyright (C) 1999-2006 INRIA, Universite de Rennes 1, EDF
 *  Copyright (C) 2006-2007 Pascal Gallard - Kerlabs, Louis Rilling - Kerlabs
 */

/** Implementation of process mobility mechanisms.
 *  @file process_mobility.c
 *
 *  Implementation of functions used to migrate, duplicate and checkpoint
 *  a process.
 *
 *  @author Geoffroy Vallée
 */

#include <linux/sched.h>
#include <linux/pid.h>
#include <kerrighed/kernel_headers.h> /* just for show_registers() */

#include "debug_epm.h"

#define MODULE_NAME "ghost_process_api"

#include <asm-generic/sections.h>

#include <mm/mobility.h>
#include "action.h"
#include "application/application.h"
#include "ghost_process_management.h"


/*****************************************************************************/
/*                                                                           */
/*                              EXPORT FUNCTIONS                             */
/*                                                                           */
/*****************************************************************************/

int export_process(struct epm_action *action,
		   ghost_t *ghost,
		   struct task_struct *task,
		   struct pt_regs *regs)
{
	int r;

	DEBUG(DBG_GHOST_API, 1, "starting...\n");

	if (match_debug("epm", DBG_GHOST_API, 1)) {
		printk("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$"
		       "$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\n");
		printk("$$$ Start exporting process %s (%d: %p %p)\n",
		       task->comm, task->pid, &_stext, &_etext);
		show_registers(regs);
	}

	BUG_ON(task == NULL);

	r = export_task(action, ghost, task, regs);
	if (r)
		goto error;

	r = export_application(action, ghost, task);
	if (r)
		goto error;

	DEBUG(DBG_GHOST_API, 2, "sending kerrighed struct\n");

error:
	DEBUG(DBG_GHOST_API, 1, "end r=%d\n", r);
	return r;
}


/*****************************************************************************/
/*                                                                           */
/*                              IMPORT FUNCTIONS                             */
/*                                                                           */
/*****************************************************************************/

struct task_struct *import_process(struct epm_action *action,
				   ghost_t *ghost)
{
	struct task_struct *ghost_task;
	struct task_struct *active_task;
	struct pt_regs regs;
	int err;

	DEBUG(DBG_GHOST_API, 1, "Starting...\n");

	/* Process importation */
	DEBUG(DBG_GHOST_API, 2, "action=%d\n", action->type);

	if (action->type == EPM_MIGRATE) {
		/* Ensure that no task struct survives from a previous stay of
		 * the process on this node.
		 * This can happen if a process comes back very quickly
		 * and before the call to do_exit_wo_notify() ending
		 * the previous migration.
		 */
		struct pid *pid;

		pid = find_get_pid(action->migrate.pid);
		if (pid) {
			rcu_read_lock();
			while (pid_task(pid, PIDTYPE_PID)) {
				rcu_read_unlock();
				schedule();
				rcu_read_lock();
			}
			rcu_read_unlock();
			put_pid(pid);
		}
	}

	ghost_task = import_task(action, ghost, &regs);
	if (IS_ERR(ghost_task)) {
		err = PTR_ERR(ghost_task);
		goto err_task;
	}
	BUG_ON(!ghost_task);

#ifdef CONFIG_KRG_DEBUG
	if (match_debug("epm", DBG_GHOST_API, 1)) {
		printk("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$"
		       "$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\n");
		printk("$$$ Start importing process %p %p\n", &_stext, &_etext);
		show_registers(&regs);
	}
#endif

	active_task = create_new_process_from_ghost(ghost_task, &regs, action);
	if (IS_ERR(active_task)) {
		err = PTR_ERR(active_task);
		goto err_active_task;
	}
	BUG_ON(!active_task);

	free_ghost_process(ghost_task);

	err = import_application(action, ghost, active_task);
	if (err)
		goto err_application;

	DEBUG(DBG_GHOST_API, 1, "Done\n");

	return active_task;

 err_application:
	unimport_application(action, ghost, active_task);
 err_active_task:
	unimport_task(action, ghost_task);
 err_task:
	DEBUG(DBG_GHOST_API, 1, "end r=%d\n", err);
	return ERR_PTR(err);
}
