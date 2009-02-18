/** Process distant syscall API interface.
 *  @file distant_syscall.h
 *
 *  Definition of process distant syscalls interface.
 *  @author Geoffroy Vallee
 */

#ifndef __DISTANT_SYSCALLS_H__
#define __DISTANT_SYSCALLS_H__

#ifdef CONFIG_KRG_PROC

#include <linux/types.h>
#include <kerrighed/sys/types.h>
#include <kerrighed/sys/capabilities.h>
#ifdef CONFIG_KRG_EPM
#include <epm/migration_api.h>
#endif

struct caller_creds;

/*--------------------------------------------------------------------------*
 *                                                                          *
 *                              EXTERN FUNCTIONS                            *
 *                                                                          *
 *--------------------------------------------------------------------------*/

int krg_wake_up_process(pid_t pid);

int kcb_get_pid_cap(pid_t pid, struct caller_creds *requester_creds,
		    krg_cap_t *requested_cap);
int kcb_set_pid_cap(pid_t pid, struct caller_creds *requester_creds,
		    krg_cap_t * requested_cap);

#ifdef CONFIG_KRG_EPM
int kcb_migrate_process(pid_t pid,
			enum migration_scope scope,
			kerrighed_node_t destination_node_id,
			struct caller_creds *requester_creds);

/* Not implemented */
int kcb_checkpoint_process(pid_t pid,
			   struct caller_creds *requester_creds, int action);
#endif /* CONFIG_KRG_EPM */

void pgrp_is_cluster_wide(pid_t pgrp);

int is_pgrp_cluster_wide(pid_t pgrp);

int distant_wake_up_process(pid_t kid);

#endif /* CONFIG_KRG_PROC */

#endif /* __DISTANT_SYSCALLS_H__ */
