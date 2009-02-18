#ifndef __PROCESS_SET_MOBILITY_H__
#define __PROCESS_SET_MOBILITY_H__

#include <linux/pid.h>
#include <ghost/ghost_types.h>

struct epm_action;
struct task_struct;

int export_process_set_links_start(struct epm_action *action, ghost_t *ghost,
				   struct task_struct *task);
int export_process_set_links(struct epm_action *action, ghost_t *ghost,
			     struct pid *pid, enum pid_type type);
int export_process_set_links_end(struct epm_action *action, ghost_t *ghost,
				 struct task_struct *task);

static inline
int import_process_set_links_start(struct epm_action *action, ghost_t *ghost,
				   struct task_struct *task)
{
	return 0;
}
int import_process_set_links(struct epm_action *action, ghost_t *ghost,
			     struct pid *pid, enum pid_type type);
int import_process_set_links_end(struct epm_action *action, ghost_t *ghost,
				 struct task_struct *task);

#endif /* __PROCESS_SET_MOBILITY_H__ */
