#ifndef __PROC_PID_H__
#define __PROC_PID_H__

#ifdef CONFIG_KRG_EPM

struct pid;
struct task_kddm_object;
struct pid_kddm_object;

/* Must be called under rcu_read_lock() */
struct task_kddm_object *krg_pid_task(struct pid *pid);

/* Must be called under rcu_read_lock() */
void pid_unlink_task(struct pid_kddm_object *obj);

#endif

#endif /* __PROC_PID_H__ */
