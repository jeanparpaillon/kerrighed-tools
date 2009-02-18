/** Processor load computation.
 *  @file mosix_probe.h
 *
 *  Implementation of processor load computation functions.
 *  It is a simplified version of the MOSIX functions.
 *
 *  Original work by Amnon Shiloh and Amnon Barak.
 *
 *  @author Louis Rilling
 */

#ifndef __MOSIX_PROBE_H__
#define __MOSIX_PROBE_H__

#ifdef CONFIG_KRG_SCHED_MOSIX_PROBE

#include <kerrighed/sys/types.h>

struct task_struct;
struct krg_sched_info;

extern unsigned long mosix_mean_load;
extern unsigned long mosix_upper_load;
extern unsigned long mosix_single_process_load;
extern unsigned long mosix_norm_mean_load;
extern unsigned long mosix_norm_upper_load;
extern unsigned long mosix_norm_single_process_load;

void mosix_probe_init(void);

int mosix_probe_init_info(struct task_struct *task,
			  struct krg_sched_info *info);
static inline void mosix_probe_cleanup_info(struct task_struct *task,
					    struct krg_sched_info *info)
{
}

unsigned long eval_load_on_remote_node(struct task_struct *tsk,
				       kerrighed_node_t node_id);

#endif /* CONFIG_KRG_SCHED_MOSIX_PROBE */

#endif /* __MOSIX_PROBE_H__ */
