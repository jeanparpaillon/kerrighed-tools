/** Process migration API interface.
 *  @file migration.h
 *
 *  Definition of process migration interface.
 *  @author Geoffroy Vallée
 */

#ifndef __MIGRATION_API_H__
#define __MIGRATION_API_H__

#ifdef CONFIG_KRG_EPM

#include <linux/types.h>
#include <kerrighed/sys/types.h>

/*--------------------------------------------------------------------------*
 *                                                                          *
 *                                   TYPES                                  *
 *                                                                          *
 *--------------------------------------------------------------------------*/

enum migration_scope {
	MIGR_THREAD,		/* Migration of one linux thread */
	MIGR_LOCAL_PROCESS,	/* Migration of all local linux threads of a
				 * process */
	MIGR_GLOBAL_PROCESS,	/* Migration of all linux threads (even those
				 * running on other nodes) of a process */
};

struct caller_creds;


/*--------------------------------------------------------------------------*
 *                                                                          *
 *                              EXTERN FUNCTIONS                            *
 *                                                                          *
 *--------------------------------------------------------------------------*/

int __migrate_linux_threads(struct task_struct *task_to_migrate,
			    enum migration_scope migr_scope,
			    kerrighed_node_t dest_node,
			    struct caller_creds *requester_creds);
int migrate_linux_threads(pid_t pid,
			  enum migration_scope migr_scope,
			  kerrighed_node_t dest_node,
			  struct caller_creds *requester_creds);

/** System call to migrate a thread.
 *  @author Geoffroy Vallee
 *
 *  @param pid        pid of the thread to migrate.
 *  @param dest_node  Id of the node to migrate the process to.
 */
int sys_migrate_thread(pid_t pid, kerrighed_node_t dest_node);

/** System call to migrate a task
 *  @author Geoffroy Vallee
 *
 *  @param tgid       tgid of the process to migrate.
 *  @param dest_node  Id of the node to migrate the process to.
 */
int sys_migrate_process(pid_t tgid, kerrighed_node_t dest_node);

#endif /* CONFIG_KRG_EPM */

#endif /* __MIGRATION_API_H__ */
