#ifndef __HOTPLUG_INTERNAL__
#define __HOTPLUG_INTERNAL__

extern struct workqueue_struct *krg_ha_wq;

extern struct work_struct fail_work;

int hooks_start(void);
void hooks_stop(void);

int __nodes_add(struct hotplug_node_set *node_set);

int repair_monitor(void);
void update_heartbeat(void);

int krgnodemask_copy_from_user(krgnodemask_t *dst, __krgnodemask_t *from);

int heartbeat_init(void);
int hotplug_add_init(void);
int hotplug_remove_init(void);
int hotplug_failure_init(void);
int hotplug_hooks_init(void);
int hotplug_cluster_init(void);

int hotplug_membership_init(void);
void hotplug_membership_cleanup(void);

#endif
