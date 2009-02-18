/*
 *  Kerrighed/modules/epm/epm.c
 *
 *  Copyright (C) 1999-2006 INRIA, Universite de Rennes 1, EDF
 */

#include <linux/kernel.h>
#include <kerrighed/ghost.h>
#include <kerrighed/hashtable.h>
#include <kerrighed/sched.h>


#define MODULE_NAME "EPM"

#include <hotplug/hotplug.h>
#include "epm_internal.h"
#include "migration.h"
#include "checkpoint.h"
#include "application/application.h"
#ifdef CONFIG_KRG_FD
#include "fork_delay.h"
#endif				//CONFIG_KRG_FD

#include "debug_epm.h"

int epm_hotplug_init(void);
void epm_hotplug_cleanup(void);

static int kcb_copy_application(struct task_struct *task)
{
	int r = 0;
	task->application = NULL;

	DEBUG(DBG_APPLICATION, 4, "Begin - Pid %d\n", task->pid);

	/* father is no more checkpointable ? */
	if (!cap_raised(current->krg_cap_effective, CAP_CHECKPOINTABLE) &&
	    current->application)
		unregister_task_to_app(current->application, current);


	/* father has not CHECKPOINTABLE as an inheritable capability
	 * (at this stage of fork, our capability are still father capability) */
	if (!cap_raised(current->krg_cap_inheritable_effective,
			CAP_CHECKPOINTABLE))
		return 0;

	/* father is CHECKPOINTABLE but is not associatied to an application,
	 * fix it ! */
	if (cap_raised(current->krg_cap_effective, CAP_CHECKPOINTABLE) &&
	    !current->application)
		r = create_application(current);

	if (r)
		goto err;

	if (current->application)
		r = register_task_to_app(current->application, task);

	/* the following can be done only when needed. Doing this will optimize
	   the forking time */
	/* else
	   r = create_application(task);*/

err:
	DEBUG(DBG_APPLICATION, 4, "End - Pid %d, r=%d\n", task->pid, r);

	return r;
}

static void kcb_exit_application(struct task_struct *task)
{
	if (task->application)
		unregister_task_to_app(task->application, task);
}

int init_epm(void)
{
	printk("EPM initialisation: start\n");

	init_epm_debug();

	restart_block_krgsyms_register();

	register_krg_ptrace_hooks();

	epm_migration_start();
	register_migration_hooks();

	register_checkpoint_hooks();

	epm_procfs_start();

	application_cr_server_init();

#ifdef CONFIG_KRG_FD
	epm_fork_delay_start();
	register_fork_delay_hooks();
#endif
	hook_register(&kh_copy_application, kcb_copy_application);
	hook_register(&kh_exit_application, kcb_exit_application);

	epm_hotplug_init();

	printk("EPM initialisation: done\n");
	return 0;
}

void cleanup_epm(void)
{
	epm_hotplug_cleanup();

#ifdef CONFIG_KRG_FD
	epm_fork_delay_exit();
#endif

	application_cr_server_finalize();

	epm_procfs_exit();

	epm_migration_exit();

	restart_block_krgsyms_unregister();
}
