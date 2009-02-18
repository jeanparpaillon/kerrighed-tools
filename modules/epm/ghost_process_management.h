#ifndef __GHOST_PROCESS_MANAGEMENT_H__
#define __GHOST_PROCESS_MANAGEMENT_H__

#ifdef CONFIG_KRG_EPM

#include <ghost/ghost.h>

struct task_struct;
struct pt_regs;
struct epm_action;

void free_ghost_process(struct task_struct *ghost);

struct task_struct *create_new_process_from_ghost(struct task_struct *,
						  struct pt_regs *,
						  struct epm_action *action);

int export_task(struct epm_action *action,
		ghost_t *ghost,
		struct task_struct *task,
		struct pt_regs *regs);
struct task_struct *import_task(struct epm_action *action,
				ghost_t *ghost,
				struct pt_regs *l_regs);
void unimport_task(struct epm_action *action, struct task_struct *ghost_task);

#endif /* CONFIG_KRG_EPM */

#endif /* __GHOST_PROCESS_MANAGEMENT_H__ */
