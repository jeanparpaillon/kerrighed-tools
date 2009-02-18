/*
 *  Copyright (C) 2006-2007, Pascal Gallard, Kerlabs.
 */

#define MODULE_NAME "Hotplug"

#include <linux/sched.h>
#include <linux/rwsem.h>
#include <linux/mutex.h>
#include <linux/completion.h>
#include <linux/string.h>
#include <asm/uaccess.h>
#include <kerrighed/sys/types.h>
#include <kerrighed/krginit.h>
#include <kerrighed/hotplug.h>
#include <kerrighed/krgnodemask.h>

#include <kerrighed/krgflags.h>

#include <tools/krg_syscalls.h>
#include <tools/krg_services.h>
#include <tools/workqueue.h>
#include <rpc/rpc.h>
#ifdef CONFIG_KRG_CTNR
#include <ctnr/kddm.h>
#endif
#ifdef CONFIG_KRG_PROC
#include <kerrighed/task.h>
#include <kerrighed/pid.h>
#include <proc/task.h>
#include <proc/signal_management.h>
#ifdef CONFIG_KRG_EPM
#include <proc/sighand_management.h>
#include <proc/children.h>
#endif
#endif
#ifdef CONFIG_KRG_SCHED_CONFIG
#include <scheduler/core/krg_sched_info.h>
#endif

#include "hotplug.h"
#include "hotplug_internal.h"

#define ADVERTISE_PERIOD (2*HZ)
#define UNIVERSE_PERIOD (60*HZ)

enum {
	CLUSTER_UNDEF,
	CLUSTER_DEF,
};

static char clusters_status[KERRIGHED_MAX_CLUSTERS];

struct hotplug_node_set cluster_start_node_set;
static int cluster_start_in_progress;
static DEFINE_SPINLOCK(cluster_start_lock);
static DEFINE_MUTEX(cluster_start_mutex);
static DECLARE_COMPLETION(cluster_started);

static void init_prekerrighed_process(void)
{
#ifdef CONFIG_KRG_PROC
	struct task_struct *g, *t;
#endif

	down_write(&kerrighed_init_sem);
	read_lock(&tasklist_lock);

#ifdef CONFIG_KRG_PROC
	/* Initialize location structure for running processes */
	do_each_thread(g, t) {
		/* Just make sure that our assumptions remain true */
		BUG_ON(t->tgid != g->pid);
		BUG_ON(t->group_leader->pid != g->pid);

		if (!(t->pid & GLOBAL_PID_MASK) || !t->mm) {
			BUG_ON(t->task_obj);
			BUG_ON(t->signal->krg_objid);
#ifdef CONFIG_KRG_EPM
			BUG_ON(t->sighand->krg_objid);
			BUG_ON(t->children_obj);
#endif
#ifdef CONFIG_KRG_SCHED_CONFIG
			BUG_ON(t->krg_sched);
#endif
			continue;
		}

		kcb_fill_task_kddm_object(t);
		if (likely(kcb_malloc_signal_struct(t, 1)))
			kcb_signal_struct_unlock(t->tgid);
#ifdef CONFIG_KRG_EPM
		kcb_malloc_sighand_struct(t, 1);
		kcb_fill_children_kddm_object(t);
#endif
#ifdef CONFIG_KRG_SCHED_CONFIG
		fill_krg_sched_info(t);
#endif
	} while_each_thread(g, t);
#endif /* CONFIG_KRG_PROC */

	hooks_start();

	read_unlock(&tasklist_lock);
	up_write(&kerrighed_init_sem);
};

static void handle_cluster_start(struct rpc_desc *desc, void *data, size_t size)
{
	struct hotplug_node_set start_msg;
	int ret = 0;
	int err;

	mutex_lock(&cluster_start_mutex);

	memcpy(&start_msg, data, sizeof(start_msg));
	if (kerrighed_subsession_id != -1){
		printk("WARNING: Rq to add me in a cluster (%d) when I'm already in one (%d)\n",
		       start_msg.subclusterid, kerrighed_subsession_id);
		goto cancel;
	}
	err = rpc_pack_type(desc, ret);
	if (err)
		goto cancel;
	err = rpc_unpack_type(desc, ret);
	if (err)
		goto cancel;

	init_prekerrighed_process();

	__nodes_add(&start_msg);

	rpc_enable_all();

	SET_KERRIGHED_CLUSTER_FLAGS(KRGFLAGS_RUNNING);
	SET_KERRIGHED_NODE_FLAGS(KRGFLAGS_RUNNING);
	clusters_status[kerrighed_subsession_id] = CLUSTER_DEF;

	printk("Kerrighed is running on %d nodes\n", num_online_krgnodes());
	complete_all(&cluster_started);

out:
	mutex_unlock(&cluster_start_mutex);
	return;

cancel:
	rpc_cancel(desc);
	goto out;
}

static void cluster_start_worker(struct work_struct *work)
{
	struct rpc_desc *desc;
	kerrighed_node_t node;
	int ret;
	int err;

	desc = rpc_begin_m(CLUSTER_START, &cluster_start_node_set.v);
	if (!desc)
		goto out;
	err = rpc_pack_type(desc, cluster_start_node_set);
	if (err)
		goto end;
	for_each_krgnode_mask(node, cluster_start_node_set.v) {
		err = rpc_unpack_type_from(desc, node, ret);
		if (err)
			goto cancel;
	}
	ret = 0;
	err = rpc_pack_type(desc, ret);
	if (err)
		goto cancel;
	/*
	 * We might wait for a last ack from the nodes, but there would be no
	 * gain since local cluster start is currently not allowed to fail and
	 * transactions will be queued until the nodes are ready.
	 */
end:
	rpc_end(desc, 0);
out:
	spin_lock(&cluster_start_lock);
	cluster_start_in_progress = 0;
	spin_unlock(&cluster_start_lock);
	return;
cancel:
	rpc_cancel(desc);
	goto end;
}

static DECLARE_WORK(cluster_start_work, cluster_start_worker);

static int do_cluster_start(const struct hotplug_node_set *node_set)
{
	int r = -EALREADY;

	spin_lock(&cluster_start_lock);
	if (!cluster_start_in_progress) {
		r = 0;
		cluster_start_in_progress = 1;
	}
	spin_unlock(&cluster_start_lock);
	if (!r) {
		cluster_start_node_set = *node_set;
		queue_work(krg_wq, &cluster_start_work);
	}
	return r;
}

static void do_cluster_wait_for_start(void)
{
	wait_for_completion(&cluster_started);
}

static int cluster_start(void *arg)
{
	int r = 0;
	struct __hotplug_node_set __node_set;
	struct hotplug_node_set node_set;

	if (copy_from_user(&__node_set, arg, sizeof(__node_set))) {
		r = -EFAULT;
	} else {
		node_set.subclusterid = __node_set.subclusterid;

		if (krgnodemask_copy_from_user(&node_set.v, &__node_set.v))
			r = -EFAULT;
		else
			r = do_cluster_start(&node_set);
	}
	if (!r)
		do_cluster_wait_for_start();

	return r;
}

void cluster_autostart(void)
{
	struct hotplug_node_set node_set;
	static int already_start = 0;
	int i, nb;

	if (likely(already_start)
	   || kerrighed_nb_nodes_min < 0)
		return;

	node_set.subclusterid = 0;
	nb = 0;

	krgnodes_clear(node_set.v);
	for (i=0; i<KERRIGHED_MAX_NODES; i++) {
		if ((universe[i].state == 0
		    && i != kerrighed_node_id)
		    || universe[i].subid != -1)
			continue;

		if (i < kerrighed_node_id)
			return;

		nb++;
		krgnode_set(i, node_set.v);
	}

	if (nb >= kerrighed_nb_nodes_min) {
		already_start = 1;
		do_cluster_start(&node_set);
	}
}

static int cluster_wait_for_start(void __user *arg)
{
	do_cluster_wait_for_start();
	return 0;
}

static int cluster_restart(void *arg)
{
	int unused;

	rpc_async_m(NODE_FAIL, &krgnode_online_map,
		    &unused, sizeof(unused));
	
	return 0;
}

static int cluster_stop(void *arg)
{
	int unused;
	
	rpc_async_m(NODE_FAIL, &krgnode_online_map,
		    &unused, sizeof(unused));
	
	return 0;
}

static int cluster_status(void __user *arg)
{
	int r = -EFAULT;
	struct hotplug_clusters __user *uclusters = arg;
	int bcl;

	if (!access_ok(VERIFY_WRITE, uclusters, sizeof(*uclusters)))
		goto out;

	for (bcl = 0; bcl < KERRIGHED_MAX_CLUSTERS; bcl++)
		if (__put_user(clusters_status[bcl], &uclusters->clusters[bcl]))
			goto out;
	r = 0;

out:
	return r;
}

static int cluster_nodes(void __user *arg)
{
	int r = -EFAULT;
	struct hotplug_nodes __user *nodes_arg = arg;
	char __user *unodes;
	char state;
	int bcl;

	if (get_user(unodes, &nodes_arg->nodes))
		goto out;

	if (!access_ok(VERIFY_WRITE, unodes, KERRIGHED_MAX_NODES))
		goto out;

	for (bcl = 0; bcl < KERRIGHED_MAX_NODES; bcl++) {
		state = 0;
		if (krgnode_present(bcl) && universe[bcl].state)
			state = 1;
		if (__put_user(state, &unodes[bcl]))
			goto out;
	}
	r = 0;

out:
	return r;
}

int krgnodemask_copy_from_user(krgnodemask_t *dstp, __krgnodemask_t *srcp)
{
	int r;

	r = find_next_bit(srcp->bits, KERRIGHED_HARD_MAX_NODES,
			  KERRIGHED_MAX_NODES);

	if (r >= KERRIGHED_MAX_NODES && r < KERRIGHED_HARD_MAX_NODES) {
		printk("Warning: there are some bits after KERRIGHED_MAX_NODES (%d/%d/%d)\n",
		       r, KERRIGHED_MAX_NODES, KERRIGHED_HARD_MAX_NODES);
		printk("Not all the requested nodes will be started\n");
	}

	bitmap_copy(dstp->bits, srcp->bits, KERRIGHED_MAX_NODES);

	return 0;
}

int hotplug_cluster_init(void)
{
	int bcl;
	
	for (bcl = 0; bcl < KERRIGHED_MAX_CLUSTERS; bcl++) {
		clusters_status[bcl] = CLUSTER_UNDEF;
	}

	rpc_register_void(CLUSTER_START, handle_cluster_start, 0);
	
	register_proc_service(KSYS_HOTPLUG_START, cluster_start);
	register_proc_service(KSYS_HOTPLUG_WAIT_FOR_START,
			      cluster_wait_for_start);
	register_proc_service(KSYS_HOTPLUG_SHUTDOWN, cluster_stop);
	register_proc_service(KSYS_HOTPLUG_RESTART, cluster_restart);
	register_proc_service(KSYS_HOTPLUG_STATUS, cluster_status);
	register_proc_service(KSYS_HOTPLUG_NODES, cluster_nodes);

	kh_cluster_autostart = cluster_autostart;
	return 0;
}

void hotplug_cluster_cleanup(void)
{
}
