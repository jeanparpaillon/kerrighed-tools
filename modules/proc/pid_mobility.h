#ifndef __PID_MOBILITY_H__
#define __PID_MOBILITY_H__

#ifdef CONFIG_KRG_EPM

#include <linux/types.h>
#include <linux/pid.h>

#include <ghost/ghost_types.h>

struct epm_action;
struct task_struct;

int export_pid(struct epm_action *action,
	       ghost_t *ghost, struct pid_link *link);
int export_pid_namespace(struct epm_action *action,
			 ghost_t *ghost, struct task_struct *task);

int import_pid(struct epm_action *action,
	       ghost_t *ghost, struct pid_link *link);
int import_pid_namespace(struct epm_action *action,
			 ghost_t *ghost, struct task_struct *task);

int unimport_pid_namespace(struct task_struct *task);
void unimport_pid(struct pid_link *link);

/* Used by checkpoint/restart */
int reserve_pid(pid_t pid);
int pid_link_task(pid_t pid);
int cancel_pid_reservation(pid_t pid);

#endif /* CONFIG_KRG_EPM */

#endif /* __PID_MOBILITY_H__ */
