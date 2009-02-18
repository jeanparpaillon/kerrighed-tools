/*
 *  Kerrighed/modules/scheduler/core/core.c
 *
 *  Copyright (C) 2007 Marko Novak - Xlab
 *  Copyright (C) 2007-2008 Louis Rilling - Kerlabs
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/configfs.h>

#include <hotplug/hotplug.h>
#include "internal.h"
#include "process_set.h"

static struct config_item_type krg_scheduler_type = {
	.ct_owner = THIS_MODULE,
};

struct configfs_subsystem krg_scheduler_subsys = {
	.su_group = {
		.cg_item = {
			.ci_namebuf = "krg_scheduler",
			.ci_type = &krg_scheduler_type,
		}
	}
};

int init_scheduler(void)
{
	int ret;
	struct config_group **defs = NULL;

	/* per task informations framework */
	ret = krg_sched_info_start();
	if (ret)
		goto err_krg_sched_info;

	/* initialize global mechanisms to replicate configfs operations */
	ret = global_lock_start();
	if (ret)
		goto err_global_lock;
	ret = string_list_start();
	if (ret)
		goto err_string_list;
	ret = global_config_start();
	if (ret)
		goto err_global_config;
	ret = remote_pipe_start();
	if (ret)
		goto err_remote_pipe;

	/* initialize and register configfs subsystem. */
	config_group_init(&krg_scheduler_subsys.su_group);
	init_MUTEX(&krg_scheduler_subsys.su_sem);

	/* add probes, sched_policies to scheduler. */
	defs = kcalloc(3, sizeof (struct config_group *), GFP_KERNEL);

	if (defs == NULL) {
		printk(KERN_ERR "[%s] error: cannot allocate memory!\n",
			"scheduler_module_init");
		ret = -ENOMEM;
		goto err_kcalloc;
	}

	/* initialize probes and scheduling policies subgroup. */
	defs[0] = scheduler_probe_start();
	defs[1] = scheduler_start();
	defs[2] = NULL;

	if (defs[0]==NULL || defs[1]==NULL) {
		printk(KERN_ERR "[%s] error: Could not initialize one of the"
			" subgroups!\n", __PRETTY_FUNCTION__);
		ret = -EFAULT;
		goto err_init;
	}

	krg_scheduler_subsys.su_group.default_groups = defs;

	ret = configfs_register_subsystem(&krg_scheduler_subsys);

	if (ret) {
		printk(KERN_ERR "[%s] error %d: cannot register subsystem!\n",
			__PRETTY_FUNCTION__, ret);
		goto err_register;
	}

	printk(KERN_INFO "scheduler initialization succeeded!\n");
	return 0;

	configfs_unregister_subsystem(&krg_scheduler_subsys);
err_register:

err_init:
	if (defs[1])
		scheduler_exit();
	if (defs[0])
		scheduler_probe_exit();
	kfree(defs);
err_kcalloc:

	remote_pipe_exit();
err_remote_pipe:

	global_config_exit();
err_global_config:

	string_list_exit();
err_string_list:

	global_lock_exit();
err_global_lock:

	krg_sched_info_exit();
err_krg_sched_info:

	return ret;
}

void cleanup_scheduler(void)
{
}
