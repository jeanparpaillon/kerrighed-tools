#ifndef __IPC_MOBILITY_H__
#define __IPC_MOBILITY_H__

#include <ghost/ghost_types.h>

struct epm_action;
struct task_struct;

int export_ipc_namespace(struct epm_action *action,
			 ghost_t *ghost, struct task_struct *task);
int import_ipc_namespace(struct epm_action *action,
			 ghost_t *ghost, struct task_struct *task);
void unimport_ipc_namespace(struct task_struct *task);

int export_sysv_sem(struct epm_action *action,
		    ghost_t *ghost, struct task_struct *task);
int import_sysv_sem(struct epm_action *action,
		    ghost_t *ghost, struct task_struct *task);
void unimport_sysv_sem(struct task_struct *task);

#endif /* __IPC_MOBILITY_H__ */
