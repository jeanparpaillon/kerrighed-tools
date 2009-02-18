/*
 *  Kerrighed/modules/proc/proc.c
 *
 *  Copyright (C) 1999-2006 INRIA, Universite de Rennes 1, EDF
 *  Copyright (C) 2006-2007 Pascal Gallard - Kerlabs, Louis Rilling - Kerlabs
 */

#ifdef CONFIG_KRG_EPM
#include <linux/types.h>
#include <linux/sched.h>
#include <kerrighed/task.h>
#include <kerrighed/sched.h>
#endif

#include "debug_proc.h"

#define MODULE_NAME "proc"

#include "proc_internal.h"

#ifdef CONFIG_KRG_EPM
static void init_baby_sitter(void)
{
	baby_sitter = alloc_task_struct();
	if (baby_sitter == NULL)
		OOM;

	baby_sitter->pid = -1;
	baby_sitter->tgid = baby_sitter->pid;
	baby_sitter->state = TASK_UNINTERRUPTIBLE;
	INIT_LIST_HEAD(&baby_sitter->children);
	baby_sitter->real_parent = baby_sitter;
	baby_sitter->parent = baby_sitter;
	strncpy(baby_sitter->comm, "baby sitter", 15);
	DEBUG(DBG_MODULE, 1,
	      "created a baby_sitter at %p of pid %d\n",
	      baby_sitter, baby_sitter->pid);
}
#endif


/** Initial function of the module
 *  @author Geoffroy Vallee, Pascal Gallard
 */
int init_proc(void)
{
	printk("Proc initialisation: start\n");

	init_proc_debug();

	proc_task_start();
	proc_krg_exit_start();

	proc_signal_start();
#ifdef CONFIG_KRG_EPM
	if (krg_current != NULL) {
		printk("krg_current != NULL\n");
		BUG();
		return -1;
	}
	init_baby_sitter();
	proc_children_start();
	proc_sighand_start();
	proc_krg_fork_start();
	pid_management_start();
#endif
	proc_distant_syscalls_start();

	register_task_hooks();
	register_krg_exit_hooks();
	register_signal_hooks();
#ifdef CONFIG_KRG_EPM
	register_children_hooks();
	register_sighand_hooks();
	register_krg_fork_hooks();
	register_pid_hooks();
#endif
	register_distant_syscalls_hooks();

	printk("Proc initialisation: done\n");

	return 0;
}

void cleanup_proc(void)
{
}
