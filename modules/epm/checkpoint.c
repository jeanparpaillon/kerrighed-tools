/*
 *  Kerrighed/modules/epm/checkpoint.c
 *
 *  Copyright (C) 1999-2006 INRIA, Universite de Rennes 1, EDF
 */

/** Process checkpointing.
 *  @file checkpoint.c
 *
 *  @author Geoffroy Vallée, Matthieu Fertré
 */

#include <linux/sched.h>
#ifdef CONFIG_X86_64
#include <asm/ia32.h>
#endif
#include <linux/file.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/ptrace.h> /* for struct pt_regs */
#include <kerrighed/kerrighed_signal.h>
#include <asm/uaccess.h>

#include "debug_epm.h"

#define MODULE_NAME "checkpoint"

#include <hotplug/hotplug.h>
#include <epm/ghost_process_management.h>
#include <epm/ghost_process_api.h>
#include <epm/application/application.h>
#include <epm/action.h>
#include <epm/epm_internal.h>
#include <ctnr/kddm.h>
#include "checkpoint.h"

/*****************************************************************************/
/*                                                                           */
/*                              TOOLS FUNCTIONS                              */
/*                                                                           */
/*****************************************************************************/

int can_be_checkpointed(struct task_struct *task_to_checkpoint,
			const credentials_t *user_creds)
{
	struct caller_creds requester_creds;
	requester_creds.caller_uid = user_creds->uid;
	requester_creds.caller_euid = user_creds->euid;

	/* Check permissions */
	if (!permissions_ok(task_to_checkpoint, &requester_creds)) {
		DEBUG(DBG_CKPT, 3,
		      "[%d] Permission denied (uid:%d, %d - euid:%d, %d)\n",
		      task_to_checkpoint->pid, requester_creds.caller_uid,
		      task_to_checkpoint->uid, requester_creds.caller_euid,
		      task_to_checkpoint->euid);
		goto exit;
	}

	/* Check capabilities */
	if (!can_use_krg_cap(task_to_checkpoint, CAP_CHECKPOINTABLE)) {
		DEBUG(DBG_CKPT, 3, "[%d] Task don't have right capabilities\n",
		      task_to_checkpoint->pid);
		goto exit;
	}

	return 1; /* means true */

 exit:
	return 0; /* means false */
}


/*****************************************************************************/
/*                                                                           */
/*                            CHECKPOINT FUNCTIONS                           */
/*                                                                           */
/*****************************************************************************/

/** This function save the process information in a ghost
 *  @author Geoffroy Vallee, Renaud Lottiaux, Matthieu Fertré
 *
 *  @param task_to_checkpoint    Pointer on the task to checkpoint
 *
 *  @return 0 if everythink ok, negative value otherwise.
 */
static inline int checkpoint_task_to_ghost(struct epm_action *action,
					   ghost_t * ghost,
					   struct task_struct
					   *task_to_checkpoint,
					   struct pt_regs *regs)
{
	int r = -EINVAL;

	DEBUG(DBG_CKPT, 2, "Checkpoint task %s(%d)\n", task_to_checkpoint->comm,
	      task_to_checkpoint->pid);

	if (task_to_checkpoint == NULL) {
		PANIC("Task to checkpoint is NULL!!\n");
		goto exit;
	}

	if (regs == NULL) {
		PANIC("Regs are NULL!!\n");
		goto exit;
	}

	r = export_process(action, ghost, task_to_checkpoint, regs);

	if (krg_current != NULL)
		PANIC("krg_current not NULL\n");

	DEBUG(DBG_CKPT, 2, "public pid of the current task : %d\n",
	      current->pid);

 exit:
	DEBUG(DBG_CKPT, 2, "Checkpoint of task %s(%d) : done with error %d\n",
	      task_to_checkpoint->comm, task_to_checkpoint->pid, r);

	return r;
}


/** This function save the process information in a file
 *  @author Geoffroy Vallee, Renaud Lottiaux, Matthieu Fertré
 *
 *  @param task_to_checkpoint    Pointer on the task to checkpoint
 *
 *  @return 0 if everythink ok, negative value otherwise.
 */
static inline int
checkpoint_task_on_disk(struct epm_action *action,
			struct task_struct *task_to_checkpoint,
			struct pt_regs *regs)
{
	ghost_t *ghost;
	int r = -EINVAL;

	struct app_struct *app = task_to_checkpoint->application;
	BUG_ON(!app);

	ghost = create_file_ghost(GHOST_WRITE,
				  app->app_id,
				  app->chkpt_sn,
				  task_to_checkpoint->pid, "task");

	if (IS_ERR(ghost)) {
		r = PTR_ERR(ghost);
		goto exit;
	}

	/* Do the process ghosting */
	r = checkpoint_task_to_ghost(action, ghost,
				     task_to_checkpoint, regs);

	ghost_close(ghost);
exit:
	return r;
}


/** This function save the process information in memory
 *  @author Matthieu Fertré
 *
 *  @param task_to_checkpoint    Pointer on the task to checkpoint
 *
 *  @return 0 if everythink ok, negative value otherwise.
 */
static inline int
checkpoint_task_on_memory(struct epm_action *action,
			  struct task_struct *task_to_checkpoint,
			  struct pt_regs *regs)
{
	ghost_t *ghost;
	int r = -EINVAL;

	struct app_struct *app = task_to_checkpoint->application;
	BUG_ON(!app);

	/* Create a ghost to host the checkoint */
	ghost = create_memory_ghost(GHOST_WRITE,
				    app->app_id,
				    app->chkpt_sn,
				    task_to_checkpoint->pid, "task");

	if (IS_ERR(ghost))
		return PTR_ERR(ghost);

	/* Do the process ghosting */
	r = checkpoint_task_to_ghost(action, ghost,
				     task_to_checkpoint, regs);

	/* Close the ghost */
	ghost_close(ghost);

	return r;
}


/** This function save the process information
 *  @author Geoffroy Vallee, Renaud Lottiaux, Matthieu Fertré
 *
 *  @param task_to_checkpoint    Pointer on the task to checkpoint
 *
 *  @return 0 if everythink ok, negative value otherwise.
 */
int checkpoint_task(struct epm_action *action,
		    struct task_struct *task_to_checkpoint,
		    struct pt_regs *regs)
{
	int r = 0;
	ghost_fs_t oldfs;

	BUG_ON(!action);
	BUG_ON(!task_to_checkpoint);
	BUG_ON(!regs);

	oldfs = set_ghost_fs(task_to_checkpoint->uid, task_to_checkpoint->gid);

	/* Do the process ghosting */
	switch (action->checkpoint.media) {
	case DISK:
		r = checkpoint_task_on_disk(action, task_to_checkpoint, regs);
		break;
	case MEMORY:
		r = checkpoint_task_on_memory(action, task_to_checkpoint, regs);
		break;
	default:
		BUG();
	}

	unset_ghost_fs(&oldfs);

	return r;
}

/*****************************************************************************/
/*                                                                           */
/*                             REQUEST HELPER FUNCTIONS                      */
/*                                                                           */
/*****************************************************************************/

/* Checkpoint signal handler
 */
static void kcb_task_checkpoint(int sig, struct siginfo *info,
				struct pt_regs *regs)
{
	struct epm_action action;
	int r = 0;

	DEBUG(DBG_CKPT, 1, "%s(%d) - options: %d\n", current->comm,
	      current->pid, si_option(*info));

	/* do we really take a checkpoint ? */
	if (si_option(*info) != CHKPT_ONLY_STOP) {
		action.type = EPM_CHECKPOINT;
		action.checkpoint.media = si_media(*info);
		action.checkpoint.shared = CR_SAVE_LATER;
		r = checkpoint_task(&action, current, regs);

		DEBUG(DBG_CKPT, 1, "%s(%d): r=%d\n", current->comm, current->pid, r);
	}

	set_current_state(TASK_UNINTERRUPTIBLE);

	if (r != 0)
		set_task_chkpt_result(current, r);
	else
		set_task_chkpt_result(current, PCUS_OPERATION_OK);

	if (current->state == TASK_UNINTERRUPTIBLE)
		schedule(); /* be sure to stop now! */
}


void register_checkpoint_hooks(void)
{
	hook_register(&kh_krg_handler[KRG_SIG_CHECKPOINT], kcb_task_checkpoint);
}
