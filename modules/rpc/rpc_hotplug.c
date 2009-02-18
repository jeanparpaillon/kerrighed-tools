/*
 *  Copyright (C) 2006-2007, Pascal Gallard, Kerlabs.
 */

#include <linux/notifier.h>
#include <linux/kernel.h>
#include <kerrighed/krgnodemask.h>
#include <rpc/rpcid.h>
#include <rpc/rpc.h>
#include <rpc/rpc_internal.h>
#include <hotplug/hotplug.h>

static void rpc_remove(krgnodemask_t * vector)
{
	printk("Have to send all the tx_queue before stopping the node\n");
};


/**
 *
 * Notifier related part
 *
 */

static int rpc_notification(struct notifier_block *nb, hotplug_event_t event,
			    void *data){
	struct hotplug_node_set *node_set = data;
	
	switch(event){
	case HOTPLUG_NOTIFY_REMOVE:
		rpc_remove(&node_set->v);
		break;
	default:
		break;
	}
	
	return NOTIFY_OK;
};

int rpc_hotplug_init(void){
	register_hotplug_notifier(rpc_notification, HOTPLUG_PRIO_RPC);
	return 0;
};

void rpc_hotplug_cleanup(void){
};
