#ifndef __GHOST_ARCH_H__
#define __GHOST_ARCH_H__

#include <ghost/ghost_types.h>

struct epm_action;
struct task_struct;
struct restart_block;

/*
 * functions arch dependent
 */

void prepare_to_export(struct task_struct *task);

int export_thread_info(struct epm_action *action,
		       ghost_t *ghost, struct task_struct *task);
int import_thread_info(struct epm_action *action,
		       ghost_t *ghost, struct task_struct *task);
void unimport_thread_info(struct task_struct *task);
void free_ghost_thread_info(struct task_struct *);

int export_thread_struct(struct epm_action *action,
			 ghost_t *ghost, struct task_struct *tsk);
int import_thread_struct(struct epm_action *action,
			 ghost_t *ghost, struct task_struct *tsk);
void unimport_thread_struct(struct task_struct *task);

/*
 * generic functions needed by arch dependent stuff
 */

int export_exec_domain(struct epm_action *action,
		       ghost_t *ghost, struct task_struct *tsk);
struct exec_domain * import_exec_domain(struct epm_action *action,
					ghost_t *ghost);

int export_restart_block(struct epm_action *action,
			 ghost_t *ghost, struct task_struct *tsk);
int import_restart_block(struct epm_action *action,
			 ghost_t *ghost, struct restart_block *p);

#endif /* __GHOST_ARCH_H__ */
