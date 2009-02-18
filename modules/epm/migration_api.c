/*
 *  Kerrighed/modules/epm/migration_api.c
 *
 *  Copyright (C) 1999-2006 INRIA, Universite de Rennes 1, EDF
 *  Copyright (C) 2006-2007 Pascal Gallard - Kerlabs, Louis Rilling - Kerlabs
 */

/** Process migration API
 *  @file migration_api.c
 *
 *  @author Geoffroy Vallée
 */

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/rcupdate.h>
#include <kerrighed/sched.h>

#include "debug_epm.h"

#define MODULE_NAME "migration_api"

#include <tools/syscalls.h>
#include <proc/distant_syscalls.h>
#include "migration.h"
#include "migration_api.h"


/*****************************************************************************/
/*                                                                           */
/*                          KERNEL INTERFACE FUNCTIONS                       */
/*                                                                           */
/*****************************************************************************/

int __migrate_linux_threads(struct task_struct *task_to_migrate,
			    enum migration_scope migr_scope,
			    kerrighed_node_t dest_node,
			    struct caller_creds *requester_creds)
{
	int r = -EPERM;

	read_lock(&tasklist_lock);
	if (!__may_migrate(task_to_migrate, requester_creds))
		goto exit;

	switch (migr_scope) {
	case MIGR_THREAD:
		r = do_migrate_process(task_to_migrate, dest_node);
		break;
	case MIGR_GLOBAL_PROCESS:
		/* Until distributed threads are re-enabled, we can do it! */
#if 0
		printk("MIGR_GLOBAL_PROCESS: Not implemented\n");
		r = -ENOSYS;
		break;
#endif
	case MIGR_LOCAL_PROCESS:
	{
		struct task_struct *t;

		/* TODO: Wait until all threads are able to migrate before
		 * migrating the first one. */
		t = task_to_migrate;
		do {
			r = do_migrate_process(t, dest_node);
			if (r)
				break;
		} while ((t = next_thread(t)) != task_to_migrate);

		break;
	}
	default:
		printk("migr_scope: %d\n", migr_scope);
		BUG();
	}

 exit:
	read_unlock(&tasklist_lock);

	DEBUG(DBG_MIGR_API, 2, "Migration of (%d) : done with error %d\n",
	      task_to_migrate->pid, r);
	DEBUG(DBG_MIGR_API, 1, "stop %d\n", r);

	return r;
}
EXPORT_SYMBOL(__migrate_linux_threads);

int migrate_linux_threads(pid_t pid,
			  enum migration_scope migr_scope,
			  kerrighed_node_t dest_node,
			  struct caller_creds *requester_creds)
{
	struct task_struct *task_to_migrate;
	int r;

	/* Check the destination node */
	/* Just an optimization to avoid doing a useless remote request */
	if (!krgnode_online(dest_node))
		return -ENONET;

	rcu_read_lock();
	task_to_migrate = find_task_by_pid(pid);

	if (task_to_migrate == NULL || (task_to_migrate->flags & PF_AWAY)) {
		rcu_read_unlock();
		DEBUG(DBG_MIGR_API, 1,
		      "task (pid=%d) to migrate not found locally\n", pid);
		return kcb_migrate_process(pid, migr_scope, dest_node,
					   requester_creds);
	}

	r = __migrate_linux_threads(task_to_migrate,
				    migr_scope, dest_node,
				    requester_creds);
	rcu_read_unlock();

	return r;
}
EXPORT_SYMBOL(migrate_linux_threads);


/*****************************************************************************/
/*                                                                           */
/*                                SYS CALL FUNCTIONS                         */
/*                                                                           */
/*****************************************************************************/

/** System call to migrate a process
 *  @author Geoffroy Vallee, Pascal Gallard
 *
 *  @param tgid       tgid of the process to migrate.
 *  @param dest_node  Id of the node to migrate the process to.
 */
int sys_migrate_process(pid_t tgid, kerrighed_node_t dest_node)
{
	struct caller_creds creds;
	int r;

	DEBUG(DBG_MIGR_API, 1, "Starting %d -> %d ...\n", tgid, dest_node);

	creds.caller_uid = current->uid;
	creds.caller_euid = current->euid;

	r = migrate_linux_threads(tgid, MIGR_GLOBAL_PROCESS, dest_node, &creds);

	DEBUG(DBG_MIGR_API, 1, "Done %d -> %d r=%d\n", tgid, dest_node, r);

	return r;
}


/** System call to migrate a thread.
 *  @author Geoffroy Vallee
 *
 *  @param pid        pid of the thread to migrate.
 *  @param dest_node  Id of the node to migrate the process to.
 */
int sys_migrate_thread(pid_t pid, kerrighed_node_t dest_node)
{
	struct caller_creds creds;
	int r;

	DEBUG(DBG_MIGR_API, 1, "Starting %d -> %d ...\n", pid, dest_node);

	creds.caller_uid = current->uid;
	creds.caller_euid = current->euid;

	r = migrate_linux_threads(pid, MIGR_THREAD, dest_node, &creds);

	DEBUG(DBG_MIGR_API, 1, "Done %d -> %d r=%d\n", pid, dest_node, r);

	return r;
}
