#ifndef __SIGNAL_MANAGEMENT_H__
#define __SIGNAL_MANAGEMENT_H__

#ifdef CONFIG_KRG_PROC

#include <linux/types.h>

struct signal_struct;
struct task_struct;

struct signal_struct *kcb_signal_struct_writelock(pid_t tgid);
struct signal_struct *kcb_signal_struct_readlock(pid_t tgid);
void kcb_signal_struct_unlock(pid_t tgid);

void kcb_signal_struct_pin(struct signal_struct *sig);
void kcb_signal_struct_unpin(struct signal_struct *sig);

struct signal_struct *kcb_malloc_signal_struct(struct task_struct *task,
					       int need_update);

struct signal_struct *cr_malloc_signal_struct(pid_t tgid);
void cr_free_signal_struct(pid_t id);
pid_t __kcb_exit_signal(pid_t id);

#endif /* CONFIG_KRG_PROC */

#endif /* __SIGNAL_MANAGEMENT_H__ */
