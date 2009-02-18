#ifndef __G_SIGNAL_H__
#define __G_SIGNAL_H__

#ifdef CONFIG_KRG_EPM

#include <ghost/ghost_types.h>

struct epm_action;
struct task_struct;

extern int export_signal_struct(struct epm_action *action,
				ghost_t *ghost, struct task_struct *);
extern int import_signal_struct(struct epm_action *action,
				ghost_t *ghost, struct task_struct *);
extern void unimport_signal_struct(struct task_struct *task);
extern void free_ghost_signal(struct task_struct *);

int export_private_signals(struct epm_action *action,
			   ghost_t *ghost,
			   struct task_struct *task);
int import_private_signals(struct epm_action *action,
			   ghost_t *ghost,
			   struct task_struct *task);
void unimport_private_signals(struct task_struct *task);

#endif /* CONFIG_KRG_EPM */

#endif /* __G_SIGNAL_H__ */
