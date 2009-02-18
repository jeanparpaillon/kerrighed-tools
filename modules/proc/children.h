#ifndef __CHILDREN_H__
#define __CHILDREN_H__

#ifdef CONFIG_KRG_EPM

#include <linux/list.h>
#include <linux/types.h>
#include <linux/rwsem.h>
#include <linux/kref.h>
#include <linux/rcupdate.h>
#include <kerrighed/sys/types.h>

struct task_struct;

/* Used by hotplug */
int kcb_fill_children_kddm_object(struct task_struct *tsk);

/* Used by migration */
void leave_all_relatives(struct task_struct *tsk);
void join_local_relatives(struct task_struct *tsk);

/* Used by restart */
void kcb___share_children(struct task_struct *task);

/* Used by kcb_release_task */
void kcb_unhash_process(struct task_struct *tsk);

#endif /* CONFIG_KRG_EPM */

#endif /* __CHILDREN_H__ */
