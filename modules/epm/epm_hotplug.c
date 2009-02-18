/*
 *  Copyright (C) 2006-2007, Pascal Gallard, Kerlabs.
 */

#include <linux/sched.h>
#include <kerrighed/capabilities.h>
#include <kerrighed/krgnodemask.h>
#include <kerrighed/krginit.h>

#include <tools/syscalls.h>
#include <rpc/rpcid.h>
#include <rpc/rpc.h>
#include <rpc/rpc_internal.h>
#include <hotplug/hotplug.h>

#include <epm/migration_api.h>

/* migrate all processes that we can migrate */
static void epm_remove(krgnodemask_t * vector)
{
	struct task_struct *tsk;
	kerrighed_node_t dest_node = kerrighed_node_id;

	printk("epm_remove...\n");
	
	// Here we assume that all nodes of the cluster are not removed
	do {
		dest_node = krgnode_next_online_in_ring(dest_node);
	} while (__krgnode_isset(dest_node, vector));
	
	read_lock(&tasklist_lock);
	for_each_process(tsk) {
		if (cap_raised(tsk->krg_cap_effective, CAP_CAN_MIGRATE)) {
			// have to migrate this process
			struct caller_creds creds;

			printk("try to migrate %d %s to %d\n", tsk->pid, tsk->comm, dest_node);
			
			creds.caller_uid = 0;
			creds.caller_euid = 0;

			migrate_linux_threads(tsk->pid, MIGR_LOCAL_PROCESS,
					      dest_node, &creds);

			// Here we assume that all nodes of the cluster are not removed
			do {
				dest_node = krgnode_next_online_in_ring(dest_node);
			} while (__krgnode_isset(dest_node, vector));
			
			continue;
		};

		if (cap_raised(tsk->krg_cap_effective, CAP_USE_REMOTE_MEMORY)) {
			// have to kill this process
			printk("epm_remove: have to kill %d (%s)\n", tsk->pid,
			       tsk->comm);
			continue;
		};
	};
	read_unlock(&tasklist_lock);

};

/**
 *
 * Notifier related part
 *
 */

static int epm_notification(struct notifier_block *nb, hotplug_event_t event,
			    void *data){
	struct hotplug_node_set *node_set = data;
	
	switch(event){
	case HOTPLUG_NOTIFY_REMOVE:
		epm_remove(&node_set->v);
		break;
	default:
		break;
	}
	
	return NOTIFY_OK;
};

int epm_hotplug_init(void){
	register_hotplug_notifier(epm_notification, HOTPLUG_PRIO_EPM);
	return 0;
};

void epm_hotplug_cleanup(void){
};
