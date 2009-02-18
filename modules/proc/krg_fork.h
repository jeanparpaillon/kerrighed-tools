#ifndef __KRG_FORK_H__
#define __KRG_FORK_H__

#ifdef CONFIG_KRG_EPM
#include <ghost/ghost_types.h>

struct task_struct;
struct pt_regs;
struct epm_action;

int kcb_do_fork(unsigned long clone_flags,
		unsigned long stack_start,
		struct pt_regs *regs, unsigned long stack_size);

int export_vfork_done(struct epm_action *action,
		      ghost_t *ghost, struct task_struct *task);
int import_vfork_done(struct epm_action *action,
		      ghost_t *ghost, struct task_struct *task);
void unimport_vfork_done(struct task_struct *task);
void cleanup_vfork_done(struct task_struct *task);
#endif

#ifdef CONFIG_KRG_FD
int kcb_fd_send_msg(int code, int type);
#endif				//FD

#endif /* __KRG_FORK_H__ */
