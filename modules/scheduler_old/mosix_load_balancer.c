/*
 *  Kerrighed/modules/scheduler_old/mosix_load_balancer.c
 *
 *  Copyright (C) 2006-2007 Louis Rilling - Kerlabs
 */

/** Simplified MOSIX load balancing scheduling policy.
 *  @file mosix_load_balancer.c
 *
 *  @author Louis Rilling
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>

#include <kerrighed/sys/types.h>
#include "sched_policy.h"
#include "mosix_probe.h"
#include <procfs/dynamic_node_info_linker.h>
#include "mosix_probe_types.h"


struct mosix_load_balancer {
	struct sched_policy policy;
	int stab_factor;
};

static struct mosix_load_balancer mosix_lb = {
	.policy = {
		.name = "MosixLB",
	},
	.stab_factor = 130, /* in percents of a single process load */
};


/*  Give the load of a remote node with an extra process
 *
 *  @author Louis Rilling
 */
static unsigned long stable_remote_load(kerrighed_node_t node_id)
{
	krg_dynamic_node_info_t *ni;

	ni = get_dynamic_node_info(node_id);

	return ni->mosix_load +
		ni->mosix_single_process_load * mosix_lb.stab_factor / 100;
}


/** If some node have an estimated processor load lower than the local one,
 *  return the node having the lowest load;
 *  otherwise, return KERRIGHED_NODE_ID_NONE.
 */
kerrighed_node_t mosix_load_balancer(struct task_struct *unused)
{
	unsigned long lowest_remote_load, remote_load;
	kerrighed_node_t selected_node = KERRIGHED_NODE_ID_NONE;
	kerrighed_node_t i;

	if (!mosix_lb.policy.active)
		return KERRIGHED_NODE_ID_NONE;

	lowest_remote_load = mosix_norm_mean_load;

	for_each_online_krgnode(i){
		if (unlikely(i == kerrighed_node_id))
			continue;

		remote_load = stable_remote_load(i);
		if (remote_load < lowest_remote_load) {
			lowest_remote_load = remote_load;
			selected_node = i;
		}
	}

	return selected_node;
}


/** Control MOSIX load balancer
 *  Currently, only the stabilizing factor can be tuned.
 */

static ssize_t stab_factor_show(struct sched_policy *policy, char *buf)
{
	struct mosix_load_balancer *lb =
		container_of(policy, struct mosix_load_balancer, policy);
	char *s = buf;

	s += sprintf(s, "%d\n", lb->stab_factor);

	return (s - buf);
}


static ssize_t stab_factor_store(struct sched_policy *policy,
				 const char *buf,
				 size_t n)
{
	struct mosix_load_balancer *lb =
		container_of(policy, struct mosix_load_balancer, policy);
	unsigned long factor;

	factor = simple_strtoul(buf, NULL, 10);
	if (factor > INT_MAX)
		return -EINVAL;
	lb->stab_factor = (int) factor;
	return n;
}


SCHED_POLICY_ATTR(stab_factor, 0644, stab_factor_show, stab_factor_store);


/** Initialization / destruction of the Mosix load balancer
 */

int init_mosix_load_balancer(void)
{
	int error;

	if ((error = sched_policy_register(&mosix_lb.policy)))
		goto Error;
	if ((error = sched_policy_create_file(&mosix_lb.policy,
					      &sched_policy_attr_stab_factor)))
		goto ErrorFile;

 Done:
	return error;

 ErrorFile:
	sched_policy_unregister(&mosix_lb.policy);
 Error:
	goto Done;
}


void cleanup_mosix_load_balancer(void)
{
	sched_policy_remove_file(&mosix_lb.policy,
				 &sched_policy_attr_stab_factor);
	sched_policy_unregister(&mosix_lb.policy);
	mosix_lb.policy.active = 0;
}
