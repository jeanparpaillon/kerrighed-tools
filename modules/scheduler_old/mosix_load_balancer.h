/** Simplified MOSIX load balancing scheduling policy.
 *  @file mosix_load_balancer.h
 *
 *  @author Louis Rilling
 */

#ifndef __MOSIX_LOAD_BALANCER_H__
#define __MOSIX_LOAD_BALANCER_H__

#ifdef CONFIG_KRG_SCHED_MOSIX_PROBE

#include <kerrighed/sys/types.h>

struct task_struct;

/*  Function to find a migration target node according to
 *  MOSIX load balancing policy
 */
kerrighed_node_t mosix_load_balancer(struct task_struct *tsk);

#endif /* CONFIG_KRG_SCHED_MOSIX_PROBE */

#endif /* __MOSIX_LOAD_BALANCER_H__ */
