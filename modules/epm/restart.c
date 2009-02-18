/*
 *  Kerrighed/modules/epm/restart.c
 *
 *  Copyright (C) 1999-2006 INRIA, Universite de Rennes 1, EDF
 */

/** Process restart.
 *  @file restart.c
 *
 *  @author Geoffroy Vallée, Matthieu Fertré
 */

#include <linux/sched.h>
#include <kerrighed/pid.h>

#include <asm/uaccess.h>

#include "debug_epm.h"

#define MODULE_NAME "restart"

#include <ghost/ghost.h>
#include <epm/action.h>
#include <epm/ghost_process_api.h>
#include <epm/application/application.h>
#include <proc/pid_mobility.h>
#include "restart.h"

/** Load the process information saved in a checkpoint-file
 *  @author      Geoffroy Vallee, Renaud Lottiaux, Matthieu Fertré
 *
 *  @param pid    Pid of the task to restart
 *
 *  @return     New Tasks's UNIX PID if success,  NULL if failure
 */
static inline
struct task_struct *restart_task_from_ghost(struct epm_action *action,
					    pid_t pid,
					    ghost_t *ghost)
{
	struct task_struct *newTsk = NULL;
	int err;

	DEBUG(DBG_RESTART, 1, "restore_datas_process : starting...\n");

	/* Reserve pid */
	DEBUG(DBG_RESTART, 2, "try to reserve pidmap %d\n", pid);
	err = reserve_pid(pid);

	if (err) {
		DEBUG(DBG_RESTART, 1, "Failed to allocate %d: %d\n", pid, err);
		newTsk = ERR_PTR(-E_CR_PIDBUSY);
		goto exit;
	}

	/* Recreate the process */

	newTsk = import_process(action, ghost);
	if (IS_ERR(newTsk)) {
		DEBUG(DBG_RESTART, 1, "restarting failure: %ld\n",
		      PTR_ERR(newTsk));
		goto unimport_process;
	}
	BUG_ON(!newTsk);

	/* Link pid kddm object and task kddm obj */
	DEBUG(DBG_RESTART, 2, "Link pid kddm object and task kddm obj :"
	      "pid %d\n", pid);
	err = pid_link_task(pid);
	if (err) {
		DEBUG(DBG_RESTART, 1, "Error linking task and pid %d: %d\n",
		      pid, err);
		newTsk = ERR_PTR(err);
		goto exit;
	}

exit:
	return newTsk;

unimport_process:
/* WARNING: we must free the pid if it has been successfully reserved
 */
	printk("must free the pid\n");
	goto exit;
}


/** Load the process information saved in a checkpoint-file
 *  @author       Matthieu Fertré
 *
 *  @param pid    Pid of the task to restart
 *
 *  @return       New Task if success,  PTR_ERR if failure
 */
static inline struct task_struct *restart_task_from_disk(
	struct epm_action *action,
	pid_t pid,
	long app_id,
	int chkpt_sn)
{
	ghost_t *ghost;
	struct task_struct *task;

	ghost = create_file_ghost(GHOST_READ, app_id, chkpt_sn, pid, "task");

	if (IS_ERR(ghost))
		return ERR_PTR(PTR_ERR(ghost));

	/* Recreate the process */

	task = restart_task_from_ghost(action, pid, ghost);

	ghost_close(ghost);

	return task;
}


/** Load the process information saved in memory
 *  @author       Matthieu Fertré
 *
 *  @param pid    Pid of the task to restart
 *
 *  @return       New Task if success,  PTR_ERR if failure
 */
static inline struct task_struct *restart_task_from_memory(
	struct epm_action *action,
	pid_t pid,
	long app_id,
	int chkpt_sn)
{
	ghost_t *ghost;
	struct task_struct *task;

	/* Create the ghost */

	ghost = create_memory_ghost(GHOST_READ, app_id, chkpt_sn, pid, "task");

	if (IS_ERR(ghost))
		return ERR_PTR(PTR_ERR(ghost));

	/* Recreate the process */

	task = restart_task_from_ghost(action, pid, ghost);

	ghost_close(ghost);

	return task;
}


/** Load the process information saved
 *  @author      Matthieu Fertré
 *
 *  @param pid    Pid of the task to restart
 *
 *  @return     New Task if success,  PTR_ERR if failure
 */
static inline struct task_struct *restart_task(struct epm_action *action,
					       pid_t pid, long app_id,
					       int chkpt_sn,
					       const credentials_t * user_creds)
{
	struct task_struct *task = NULL;
	ghost_fs_t oldfs;

	oldfs = set_ghost_fs(user_creds->uid, user_creds->gid);

	switch (action->restart.media) {
	case DISK:
		task = restart_task_from_disk(action, pid, app_id, chkpt_sn);
		break;
	case MEMORY:
		task = restart_task_from_memory(action, pid, app_id, chkpt_sn);
		break;
	default:
		BUG();
	}

	unset_ghost_fs(&oldfs);
	return task;
}


/** Main kernel entry function to restart a checkpointed task.
 *  @author Geoffroy Vallee
 *
 *  @param pid              Kerrighed pid of the task to restart.
 *  @param restart_method    Method to restart process with.
 *
 *  @return task_struct or error
 */
struct task_struct * restart_process(media_t media, pid_t pid,
				     long app_id, int chkpt_sn,
				     const credentials_t * user_creds)
{
	struct epm_action action;
	struct task_struct *tsk;

	DEBUG(DBG_RESTART, 1, "process to restore %d\n", pid);

	/* Check if the process has not been already restarted */
	if (find_task_by_pid(pid) != NULL)
		return ERR_PTR(-EALREADY);

	DEBUG(DBG_RESTART, 2, "RESTART A PROCESS\n");

	action.type = EPM_CHECKPOINT;
	action.restart.media = media;
	action.restart.shared = CR_LINK_ONLY;
	action.restart.app = find_local_app(app_id);

	BUG_ON(!action.restart.app);

	tsk = restart_task(&action, pid, app_id, chkpt_sn, user_creds);

	if (IS_ERR(tsk)) {
		DEBUG(DBG_RESTART, 2, "Process (%d) restoration failed\n", pid);
		return tsk;
	}

	DEBUG(DBG_RESTART, 1, "RESTART DONE\n");

	return tsk;
}
