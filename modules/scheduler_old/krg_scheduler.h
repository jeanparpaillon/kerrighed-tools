/** Basic cluster wide process scheduler.
 *  @file krg_scheduler.h
 *
 *  @author Renaud Lottiaux
 */

#ifdef CONFIG_KRG_SCHED_LEGACY

#ifndef __KRG_SCHEDULER_H__
#define __KRG_SCHEDULER_H__

#include <kerrighed/sys/types.h>

struct task_struct;

/*--------------------------------------------------------------------------*
 *                                                                          *
 *                              EXTERN FUNCTIONS                            *
 *                                                                          *
 *--------------------------------------------------------------------------*/

void wake_up_migration_thread(void);


/** Search a remote node for migration
 *  @author Renaud Lottiaux
 *
 */
kerrighed_node_t find_a_node_according_to_the_scheduling_policy(struct
								task_struct
								*tsk);

#endif /* __KRG_SCHEDULER_H__ */

#endif /* CONFIG_KRG_SCHED_LEGACY */
