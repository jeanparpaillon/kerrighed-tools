/*
 *  Copyright (C) 2006-2007, Pascal Gallard, Kerlabs.
 */

#include <linux/workqueue.h>

#include <rpc/rpcid.h>
#include <rpc/rpc.h>

#include "hotplug.h"
#include "hotplug_internal.h"

struct workqueue_struct *krg_ha_wq;

int init_hotplug(void)
{
	krg_ha_wq = create_workqueue("krgHA");
	BUG_ON(krg_ha_wq == NULL);

	hotplug_hooks_init();

	hotplug_add_init();
	hotplug_remove_init();
	hotplug_failure_init();
	hotplug_cluster_init();
	hotplug_membership_init();

	return 0;
};

void cleanup_hotplug(void)
{
};
