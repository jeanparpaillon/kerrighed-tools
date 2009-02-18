/*
 *  Kerrighed/modules/arch/i386/ghost.c
 *
 *  Copyright (C) 1999-2006 INRIA, Universite de Rennes 1, EDF
 *  Copyright (C) 2006-2007 Pascal Gallard - Kerlabs, Louis Rilling - Kerlabs
 */

#include <linux/sched.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/i387.h>

#define MODULE_NAME "Arch Ghost"

#include "debug_i386.h"

#include <ghost/ghost.h>
#include <epm/ghost_arch.h>

struct epm_action;

void prepare_to_export(struct task_struct *task)
{
	unlazy_fpu(task);

	DEBUG(DBG_GHOST, 1, "fpu_counter=%d ti.status=%lx\n",
	      task->fpu_counter, task_thread_info(task)->status);
}

/*
 * struct thread_info
 */

int export_thread_info(struct epm_action *action,
		       ghost_t *ghost, struct task_struct *task)
{
	int r;
	r = ghost_write(ghost, task->thread_info, sizeof(struct thread_info));
	if (r)
		goto error;
	r = export_exec_domain(action, ghost, task);
	if (r)
		goto error;
	r = export_restart_block(action, ghost, task);
error:
	return r;
}

int import_thread_info(struct epm_action *action,
		       ghost_t *ghost, struct task_struct *task)
{
	struct thread_info *p;
	int r;

	p = alloc_thread_info(task);
	if (!p) {
		r = -ENOMEM;
		goto exit;
	}

	r = ghost_read(ghost, p, sizeof(struct thread_info));
	if (r)
		goto exit_free_thread_info;

	p->task = task;
	p->exec_domain = import_exec_domain(action, ghost);
	DEBUG(DBG_GHOST, 1,"%p exec_domain %p\n", p, p->exec_domain);

	p->preempt_count = 0;
	p->addr_limit = USER_DS;

	r = import_restart_block(action, ghost, &p->restart_block);
	if (r)
		goto exit_free_thread_info;

	task->thread_info = p;
exit:
	return r;

exit_free_thread_info:
	free_thread_info(p);
	goto exit;
}


void unimport_thread_info(struct task_struct *task)
{
	free_thread_info(task->thread_info);
}


void free_ghost_thread_info(struct task_struct *ghost)
{
	free_thread_info(ghost->thread_info);
}


int export_thread_struct(struct epm_action *action,
			 ghost_t *ghost, struct task_struct *tsk)
{
	int r;

	/* task_struct->thread send */
	savesegment(fs, tsk->thread.fs);

	DEBUG(DBG_GHOST, 1, "fs=0x%08lx gs=0x%08lx\n",
	      tsk->thread.fs, tsk->thread.gs);

	WARN_ON(tsk->thread.vm86_info);

	r = ghost_write(ghost, &tsk->thread, sizeof(tsk->thread));

	return r;
}


int import_thread_struct(struct epm_action *action,
			 ghost_t *ghost, struct task_struct *tsk)
{
	int r;

	/* task_struct->thread receive */
	r = ghost_read(ghost, &tsk->thread, sizeof(tsk->thread));
	if (r)
		goto err_read;

	DEBUG(DBG_GHOST, 1, "fs=0x%08lx gs=0x%08lx"
	      " fpu_counter=%d ti.status=%lx\n",
	      tsk->thread.fs, tsk->thread.gs,
	      tsk->fpu_counter, task_thread_info(tsk)->status);
err_read:
	return r;
}


void unimport_thread_struct(struct task_struct *task)
{
	/* Nothing to do */
}
