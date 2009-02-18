/** General global scheduler definitions.
 *  @file scheduler.h
 *
 *  @author Louis Rilling
 */

#ifdef CONFIG_KRG_SCHED_LEGACY

#ifndef __SCHEDULER_H__
#define __SCHEDULER_H__

#include <linux/kobject.h>
#include <linux/rcupdate.h>
#ifdef CONFIG_KRG_SCHED_MOSIX_PROBE
#include "mosix_probe_types.h"
#endif

struct krg_sched_info {
#ifdef CONFIG_KRG_SCHED_MOSIX_PROBE
	struct mosix_probe_info mosix_probe;
#endif
	struct rcu_head rcu;
};

extern struct subsystem scheduler_subsys;

#endif /* __SCHEDULER_H__ */

#endif /* CONFIG_KRG_SCHED_LEGACY */
