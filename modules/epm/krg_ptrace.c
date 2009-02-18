/*
 *  Kerrighed/modules/epm/krg_ptrace.c
 *
 *  Copyright (C) 2006-2007 Louis Rilling - Kerlabs
 */

/* Kerrighed's support for ptrace
 */

#include <linux/sched.h>
#include <linux/pid_namespace.h>
#include <linux/ptrace.h>
#include <kerrighed/ptrace.h>
#include <kerrighed/sched.h>

#define MODULE_NAME "ptrace"

#include "debug_epm.h"

#include <hotplug/hotplug.h>
#include "action.h"


static int kcb_ptrace_traceme(void)
{
	struct task_struct *tsk = current;
	int retval = 0;

	retval = krg_action_disable(tsk, EPM_MIGRATE, 0);
	if (retval)
		goto bad_task;
	DEBUG(DBG_PTRACE, 2, "D %d\n", tsk->pid);
	read_lock(&tasklist_lock);
	if (tsk->parent == baby_sitter) {
		retval = -EPERM;
		goto bad_parent;
	}
	if (tsk->parent != child_reaper(tsk)) {
		retval = krg_action_disable(tsk->parent, EPM_MIGRATE, 0);
		if (retval)
			goto bad_parent;
		DEBUG(DBG_PTRACE, 2, "D parent (%d)\n", tsk->parent->pid);
	}
	read_unlock(&tasklist_lock);

	return 0;

 bad_parent:
	read_unlock(&tasklist_lock);
	DEBUG(DBG_PTRACE, 2, "E %d\n", tsk->pid);
	krg_action_enable(tsk, EPM_MIGRATE, 0);
 bad_task:
	return retval;
}


static int kcb_ptrace_attach(struct task_struct *task)
{
	struct task_struct *parent;
	int retval;

	/* Lock to-be-ptraced task on this node */
	retval = krg_action_disable(task, EPM_MIGRATE, 0);
	if (retval)
		goto bad_task;
	DEBUG(DBG_PTRACE, 2, "D %d\n", task->pid);
	/* Lock ptracer on this node */
	retval = krg_action_disable(current, EPM_MIGRATE, 0);
	if (retval)
		goto bad_ptracer;
	DEBUG(DBG_PTRACE, 2, "D ptracer (%d)\n", current->pid);
	/* Lock parent on this node */
	retval = -EPERM;
	parent = task->parent;
	if (parent == baby_sitter)
		goto bad_parent;
	if (parent != child_reaper(task) && parent != current) {
		retval = krg_action_disable(parent, EPM_MIGRATE, 0);
		if (retval)
			goto bad_parent;
		DEBUG(DBG_PTRACE, 2, "D parent (%d)\n", parent->pid);
	}

	return 0;

 bad_parent:
	DEBUG(DBG_PTRACE, 2, "E ptracer (%d)\n", current->pid);
	krg_action_enable(current, EPM_MIGRATE, 0);
 bad_ptracer:
	DEBUG(DBG_PTRACE, 2, "E %d\n", task->pid);
	krg_action_enable(task, EPM_MIGRATE, 0);
 bad_task:
	return retval;
}


static void kcb_ptrace_detach(struct task_struct *task)
{
	/* TODO: Need more work for multi-threaded parents */
	read_lock(&tasklist_lock);
	BUG_ON(task->parent == baby_sitter);
	if (task->parent != child_reaper(task) && task->parent != current) {
		DEBUG(DBG_PTRACE, 2, "E parent (%d)\n", task->parent->pid);
		krg_action_enable(task->parent, EPM_MIGRATE, 0);
	}
	read_unlock(&tasklist_lock);
	DEBUG(DBG_PTRACE, 2, "E ptracer (%d)\n", current->pid);
	krg_action_enable(current, EPM_MIGRATE, 0);
	DEBUG(DBG_PTRACE, 2, "E %d\n", task->pid);
	krg_action_enable(task, EPM_MIGRATE, 0);
}


/* Assumes at least read_lock on tasklist */
/* Called with write_lock_irq on tasklist */
static void kcb_ptrace_ptracer_exit(struct task_struct *task)
{
	BUG_ON(task->real_parent == baby_sitter);
	if (task->real_parent != child_reaper(task)
	    && task->real_parent != task->parent) {
		DEBUG(DBG_PTRACE, 2, "E parent (%d)\n", task->real_parent->pid);
		krg_action_enable(task->real_parent, EPM_MIGRATE, 0);
	}
	/* Not really needed as long as zombies do not migrate... */
	DEBUG(DBG_PTRACE, 2, "E ptracer (%d)\n", task->parent->pid);
	krg_action_enable(task->parent, EPM_MIGRATE, 0);
	DEBUG(DBG_PTRACE, 2, "E %d\n", task->pid);
	krg_action_enable(task, EPM_MIGRATE, 0);
}


/* Assumes at least read_lock on tasklist */
/* Called with write_lock_irq on tasklist */
static void kcb_ptrace_reparent_ptraced(struct task_struct *real_parent,
					struct task_struct *task)
{
	/* We do not support that the new real parent can migrate at
	 * all. This will not induce new limitations as long as threads can not
	 * migrate. */

	/* Not really needed as long as zombies do not migrate... */
	DEBUG(DBG_PTRACE, 2, "E parent (%d)\n", real_parent->pid);
	krg_action_enable(real_parent, EPM_MIGRATE, 0);
	/* new real_parent has already been assigned. */
	BUG_ON(task->real_parent == baby_sitter);
	if (task->real_parent != child_reaper(task)
	    && task->real_parent != task->parent) {
		int retval;

		retval = krg_action_disable(task->real_parent, EPM_MIGRATE, 0);
		BUG_ON(retval);
		DEBUG(DBG_PTRACE, 2, "D new parent (%d)\n",
		      task->real_parent->pid);
	}
}


/* Assumes at least read_lock on tasklist */
/* Called with write_lock_irq on tasklist */
static void kcb_ptrace_release_ptraced(struct task_struct *task)
{
	BUG_ON(task->real_parent == baby_sitter);
	if (task->real_parent != child_reaper(task)
	    && task->real_parent != task->parent) {
		DEBUG(DBG_PTRACE, 2, "E parent (%d)\n", task->real_parent->pid);
		krg_action_enable(task->real_parent, EPM_MIGRATE, 0);
	}
	BUG_ON(task->parent == baby_sitter);
	DEBUG(DBG_PTRACE, 2, "E ptracer (%d)\n", task->parent->pid);
	krg_action_enable(task->parent, EPM_MIGRATE, 0);
	/* No need to re-enable migration for the release task. */
}


/* Assumes at least read_lock on tasklist */
/* Called with write_lock_irq on tasklist */
static void kcb_ptrace_copy_ptrace(struct task_struct *task)
{
	int retval;

	if (unlikely(task->ptrace & PT_PTRACED)) {
		/* action_disable can not fail, since task can not have been
		 * requested to migrate yet, and migration is already disabled
		 * for parent and ptracer. */
		retval = krg_action_disable(task, EPM_MIGRATE, 0);
		BUG_ON(retval);
		DEBUG(DBG_PTRACE, 2, "D %d\n", task->pid);

		BUG_ON(task->parent == baby_sitter);
		retval = krg_action_disable(task->parent, EPM_MIGRATE, 0);
		BUG_ON(retval);
		DEBUG(DBG_PTRACE, 2, "D ptracer (%d)\n", task->parent->pid);

		BUG_ON(task->real_parent == baby_sitter);
		if (task->real_parent != child_reaper(task)
		    && task->real_parent != task->parent) {
			retval = krg_action_disable(task->real_parent,
						    EPM_MIGRATE, 0);
			BUG_ON(retval);
			DEBUG(DBG_PTRACE, 2, "D parent (%d)\n",
			      task->real_parent->pid);
		}
	}
}


void register_krg_ptrace_hooks(void)
{
	hook_register(&kh_ptrace_traceme, kcb_ptrace_traceme);
	hook_register(&kh_ptrace_attach, kcb_ptrace_attach);
	hook_register(&kh_ptrace_detach, kcb_ptrace_detach);
	hook_register(&kh_ptrace_ptracer_exit, kcb_ptrace_ptracer_exit);
	hook_register(&kh_ptrace_reparent_ptraced, kcb_ptrace_reparent_ptraced);
	hook_register(&kh_ptrace_release_ptraced, kcb_ptrace_release_ptraced);
	hook_register(&kh_ptrace_copy_ptrace, kcb_ptrace_copy_ptrace);
}
