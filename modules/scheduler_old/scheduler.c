/*
 *  Kerrighed/modules/scheduler_old/scheduler.c
 *
 *  Copyright (C) 2006-2007 Louis Rilling - Kerlabs
 */

/** Global scheduler module.
 *  @file scheduler.c
 *
 *  @author Louis Rilling
 */

#include <linux/kobject.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/rcupdate.h>
#include <kerrighed/sched.h>
#include <tools/sysfs.h>
#include <hotplug/hotplug.h>
#ifdef CONFIG_KRG_SCHED_MOSIX_PROBE
#include "mosix_probe.h"
#endif
#include "scheduler.h"

extern int init_probes(void);
extern void cleanup_probes(void);

extern int init_sched_policies(void);
extern void cleanup_sched_policies(void);

extern int init_mosix_load_balancer(void);
extern void cleanup_mosix_load_balancer(void);

extern int init_krg_scheduler(void);
extern void cleanup_krg_scheduler(void);

decl_subsys(scheduler, NULL, NULL);

static struct kmem_cache *sched_info_cachep;

static int kcb_copy_sched_info(struct task_struct *task)
{
	struct krg_sched_info *info;
	int err = 0;

	info = kmem_cache_alloc(sched_info_cachep, GFP_KERNEL);
	if (!info)
		return -ENOMEM;

#ifdef CONFIG_KRG_SCHED_MOSIX_PROBE
	err = mosix_probe_init_info(task, info);
#endif
	if (err) {
		kmem_cache_free(sched_info_cachep, info);
		info = NULL;
	}

	rcu_assign_pointer(task->krg_sched, info);
	return err;
}

static void delayed_sched_info_free(struct rcu_head *rhp)
{
	struct krg_sched_info *info =
		container_of(rhp, struct krg_sched_info, rcu);
	kmem_cache_free(sched_info_cachep, info);
}

static void kcb_free_sched_info(struct task_struct *task)
{
	struct krg_sched_info *info = rcu_dereference(task->krg_sched);

	if (!info)
		return;

	rcu_assign_pointer(task->krg_sched, NULL);
#ifdef CONFIG_KRG_SCHED_MOSIX_PROBE
	mosix_probe_cleanup_info(task, info);
#endif

	call_rcu(&info->rcu, delayed_sched_info_free);
}

int init_scheduler(void)
{
	unsigned long cache_flags = SLAB_PANIC;
	int error;

	kset_set_kset_s(&scheduler_subsys, kerrighed_subsys);
	if ((error = subsystem_register(&scheduler_subsys)))
		goto Error;
	if ((error = init_probes()))
		goto ErrorProbes;
	if ((error = init_sched_policies()))
		goto ErrorPolicies;
#ifdef CONFIG_KRG_SCHED_MOSIX_PROBE
	if ((error = init_mosix_load_balancer()))
		goto ErrorMLB;
#endif
	if ((error = init_krg_scheduler()))
		goto ErrorSched;

#ifdef CONFIG_DEBUG_SLAB
	cache_flags |= SLAB_POISON;
#endif
	sched_info_cachep = kmem_cache_create("krg_sched_info",
					       sizeof(struct krg_sched_info),
					       0, cache_flags,
					       NULL, NULL);
	if (!sched_info_cachep) {
		error = -ENOMEM;
		goto ErrorCache;
	}

	hook_register(&kh_copy_sched_info, kcb_copy_sched_info);
	hook_register(&kh_free_sched_info, kcb_free_sched_info);

 Done:
	return error;

 ErrorCache:
	cleanup_krg_scheduler();
 ErrorSched:
#ifdef CONFIG_KRG_SCHED_MOSIX_PROBE
	cleanup_mosix_load_balancer();
 ErrorMLB:
#endif
	cleanup_sched_policies();
 ErrorPolicies:
	cleanup_probes();
 ErrorProbes:
	subsystem_unregister(&scheduler_subsys);
 Error:
	goto Done;
}

void cleanup_scheduler(void)
{
	cleanup_krg_scheduler();
#ifdef CONFIG_KRG_SCHED_MOSIX_PROBE
	cleanup_mosix_load_balancer();
#endif
	cleanup_sched_policies();
	cleanup_probes();
	subsystem_unregister(&scheduler_subsys);
}
