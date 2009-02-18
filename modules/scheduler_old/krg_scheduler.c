/*
 *  Kerrighed/modules/scheduler_old/krg_scheduler.c
 *
 *  Copyright (C) 1999-2006 INRIA, Universite de Rennes 1, EDF
 *  Copyright (C) 2006-2007 Louis Rilling - Kerlabs
 */

/** Basic cluster wide process scheduler.
 *  @file krg_scheduler.c
 *
 *  @author Renaud Lottiaux
 */

#include <linux/kernel.h>
#include <linux/kmod.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/rcupdate.h>

#include <kerrighed/sys/types.h>

#include "analyzer.h"
#ifdef CONFIG_KRG_SCHED_MOSIX_PROBE
#include "mosix_load_balancer.h"
#include "mosix_probe.h"
#include "mosix_probe_types.h"
#endif
#include <tools/process.h>
#include <tools/syscalls.h>
#include <procfs/dynamic_node_info_linker.h>
#include <epm/migration.h>
#include <epm/migration_api.h>
#ifdef CONFIG_KRG_FD
#include <epm/fork_delay.h>
#endif
#include "scheduler.h"


static struct task_struct *migration_manager_task;
static DECLARE_WAIT_QUEUE_HEAD(migration_manager_wait);

/* Allow the scheduler to migrate any process */
static struct caller_creds scheduler_creds = {
	.caller_uid = 0,
	.caller_euid = 0
};


/** Find a process to migrate
 *  @author Geoffroy Vall?e, Renaud Lottiaux
 *
 *  @return   A process candidate to migrate. NULL if no process found.
 */
struct task_struct *find_a_process_to_migrate(void)
{
	struct task_struct *p;
	int highest_load, second_highest_load;
	struct task_struct *max_p, *second_max_p;
	struct krg_sched_info *info;

	highest_load = 0;
	second_highest_load = 0;
	max_p = second_max_p = NULL;

	rcu_read_lock();

	for_each_process(p) {
		/* Check if the process is alloawed to migrate */
		if (!may_migrate(p, &scheduler_creds))
			continue;

		/* Check if the process is a Kerrighed process... Hum, should not be */
		if (unlikely(!(info = rcu_dereference(p->krg_sched))))
			continue;

#ifdef CONFIG_KRG_SCHED_MOSIX_PROBE
		if (info->mosix_probe.load > highest_load) {
			second_highest_load = highest_load;
			second_max_p = max_p;
			max_p = p;
			highest_load = info->mosix_probe.load;
		} else
		    if (info->mosix_probe.load > second_highest_load) {
			second_highest_load = info->mosix_probe.load;
			second_max_p = p;
		}
#endif
	}

	if (second_max_p)
		p = second_max_p;
	else {
		if (max_p)
			p = max_p;
		else
			p = NULL;
	}

	if (p)
		get_task_struct(p);

	rcu_read_unlock();

	return p;
}


/** Search a remote node for migration
 *  @author Renaud Lottiaux
 *
 */
kerrighed_node_t
find_a_node_according_to_the_scheduling_policy(struct task_struct * tsk)
{
#ifdef CONFIG_KRG_SCHED_MOSIX_PROBE
	return mosix_load_balancer(tsk);
#else /* !CONFIG_KRG_SCHED_MOSIX_PROBE */
	return KERRIGHED_NODE_ID_NONE;
#endif
}


#ifdef CONFIG_KRG_FD
/**
 * @author Jerome Gallard
 */
struct task_struct *find_a_process_delayed_running(void)
{
	struct task_struct *p;

	for_each_process(p) {	//le for_each parcours quelle liste de processus ?
		if (p->fd_flag == 2 && p->state == TASK_RUNNING) {
			//      printk("find_a_process_delayed_running : can use FD : %d\n",p->fd_flag);
			return p;
		} else {
			//      printk("find_a_process_delayed_running : can NOT use FD : %d\n",p->fd_flag);
			continue;
		}
	}

	return NULL;
}
#endif				//CONFIG_KRG_FD


/** Migration manager thread.
 *  @author Renaud Lottiaux
 */
static int migration_manager_thread(void *dummy)
{
	DECLARE_WAITQUEUE(wait, current);
	struct sched_param sched_par;
	struct task_struct *tsk;
	int nodeid;

#ifdef CONFIG_KRG_FD
	/* FORK_DELAY */
	int fork_delay = 0;
	int load_cpu;
	kerrighed_node_t node_id;
	krg_dynamic_node_info_t *node_info;

	/* FORK_DELAY */
#endif				//CONFIG_KRG_FD

	sched_par.sched_priority = 200;	/* 200 is arbitrary */
	sched_setscheduler(current, SCHED_FIFO, &sched_par);

	while (!kthread_should_stop()) {
		set_current_state(TASK_INTERRUPTIBLE);
		add_wait_queue(&migration_manager_wait, &wait);
		schedule();
		remove_wait_queue(&migration_manager_wait, &wait);

		tsk = find_a_process_to_migrate();
		if (!tsk) {
#ifdef CONFIG_KRG_FD
			//no process_to_migrate was found
			//perhaps we could stop a PFD
			goto FORK_DELAY;
#else
			continue;
#endif
		}

		nodeid = find_a_node_according_to_the_scheduling_policy(tsk);
		if (nodeid == KERRIGHED_NODE_ID_NONE
		    || nodeid == kerrighed_node_id) {
			put_task_struct(tsk);
#ifdef CONFIG_KRG_FD
			//no node was found
			//perhaps we could stop a PFD
			goto FORK_DELAY;
#else
			continue;
#endif
		}

		notify_migration_start_to_analyzer();

		__migrate_linux_threads(tsk, MIGR_THREAD, nodeid,
					&scheduler_creds);
		put_task_struct(tsk);

		notify_migration_end_to_analyzer();

#ifdef CONFIG_KRG_FD
		if (fork_delay != 0) {
			//probleme : la migration lorsqu'elle fait un eval_node_distant()
			//ne prend pas en compte la vload
			//pas grave : mmt prend en compte le temps de maj de la sonde

		      FORK_DELAY:
			node_id = kerrighed_node_id;
			node_info = get_dynamic_node_info(node_id);
			load_cpu = node_info->mosix_load;	//ajouter offset (vload)
			if (load_cpu < threshold_scheduler) {
				continue;
			}
			tsk = find_a_process_delayed_running();
			if (!tsk) {
				//no PFD running
				//nothing better to do
				continue;
			} else {	//at least a PFD is running
				//we will stop it
				do_fork_delay_stop_process(tsk);
				continue;
			}
		}
#endif				//FD
	}

	return 0;

//version d'origine avant FORK_DELAY...
#if 0
	struct task_struct *tsk;
	int nodeid;

	kernel_thread_setup("Migration Mgr");

	current->policy = SCHED_FIFO;
	current->rt_priority = 200;	/* 200 is arbitrary */

	migration_manager_task = current;

	up(&migration_manager_thread_startsync);

	while (migration_manager_active) {
		sleep_on_task(current);

		tsk = find_a_process_to_migrate();
		if (!tsk)
			continue;

		nodeid = find_a_node_according_to_the_scheduling_policy(tsk);
		if (nodeid == KERRIGHED_NODE_ID_NONE
		    || nodeid == kerrighed_node_id)
			continue;

		notify_migration_start_to_analyzer();

		do_migrate_process(tsk, nodeid);

		notify_migration_end_to_analyzer();
	}

	up(&migration_manager_thread_stopsync);

	return 0;
#endif
//FIN version d'origine avant FORK_DELAY...
}


void wake_up_migration_thread(void)
{
	wake_up(&migration_manager_wait);
}


int init_krg_scheduler(void)
{
	/* Launch the migration manager thread */

	migration_manager_task = kthread_run(migration_manager_thread, NULL,
					     "Migration Mgr");
	if (IS_ERR(migration_manager_task))
		return PTR_ERR(migration_manager_task);
#ifdef CONFIG_KRG_SCHED_MOSIX_PROBE
	mosix_probe_init();
#endif

	return 0;
}


void cleanup_krg_scheduler(void)
{
	/* Stop the migration manager thread */
	kthread_stop(migration_manager_task);
}
