#ifndef __TASK_H__
#define __TASK_H__

#ifdef CONFIG_KRG_PROC

#include <linux/types.h>
#include <kerrighed/types.h>

/** management of process than can or have migrated
 *  @author Geoffroy Vallee, David Margery, Pascal Gallard and Louis Rilling
 */

struct task_kddm_object;
struct task_struct;

void __kcb_task_unlink(struct task_kddm_object *obj, int need_update);
void kcb_task_unlink(struct task_kddm_object *obj, int need_update);
struct task_kddm_object *kcb_task_readlock(pid_t pid);
struct task_kddm_object *kcb_task_writelock(pid_t pid);
struct task_kddm_object *kcb_task_create_writelock(pid_t pid);
void kcb_task_unlock(pid_t pid);
#ifdef CONFIG_KRG_EPM
void kcb___free_task_struct(struct task_struct *task);
#endif

/* Used by hotplug */
int kcb_fill_task_kddm_object(struct task_struct *task);

int kcb_set_pid_location(pid_t pid, kerrighed_node_t node);
#ifdef CONFIG_KRG_EPM
int kcb_unset_pid_location(pid_t pid);
#endif
kerrighed_node_t kcb_lock_pid_location(pid_t pid);
void kcb_unlock_pid_location(pid_t pid);

#endif /* CONFIG_KRG_PROC */

#endif /* __TASK_H__ */
