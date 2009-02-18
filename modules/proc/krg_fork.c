/*
 *  Kerrighed/modules/proc/krg_fork.c
 *
 *  Copyright (C) 1999-2006 INRIA, Universite de Rennes 1, EDF
 *  Copyright (C) 2006-2007 Pascal Gallard - Kerlabs, Louis Rilling - Kerlabs
 *  Copyright (C) 2008 Louis Rilling - Kerlabs
 */

/** Implement the remote process creation of a process */

#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/completion.h>
#include <kerrighed/krginit.h>
#include <kerrighed/sys/types.h>
#include <kerrighed/sched.h>
#include <asm/cpu_register.h>

#include <hotplug/hotplug.h>
#include <rpc/rpcid.h>
#include <rpc/rpc.h>
#include <ghost/ghost.h>
#include <epm/migration.h>
#include <epm/action.h>
#ifdef CONFIG_KRG_SCHED_CONFIG
#include <scheduler/core/placement.h>
#endif

struct vfork_done_proxy {
	struct completion *waiter_vfork_done;
	kerrighed_node_t waiter_node;
};

struct kmem_cache *vfork_done_proxy_cachep;

#ifdef CONFIG_KRG_FD
/** kcb_fd_send_msg
 * @author jgallard
 * send the message (code) to the fork_delay manager
 */
int kcb_fd_send_msg(int code, int type)
{
	int send_msg;
	send_msg = code;
	switch (type) {
#ifdef CONFIG_FD_V1
	case 0:
		async_remote_service_call(kerrighed_node_id, JETON,
					  FD_CHAN, &send_msg, sizeof(send_msg));
		break;
	case 1:
		async_remote_service_call(kerrighed_node_id, ADMIN,
					  FD_CHAN, &send_msg, sizeof(send_msg));
		break;
#endif				//V1
	case 2:
		async_remote_service_call(kerrighed_node_id, VLOAD,
					  FD_CHAN, &send_msg, sizeof(send_msg));
		break;
	default:
		printk("kcb_fd_send_msg : ERROR : unknown type\n");
		BUG();
	}

	return 0;		//faudra voir pour cette valeur de retour...
}
#endif				//FD


static int kcb_do_fork(unsigned long clone_flags,
		       unsigned long stack_start,
		       struct pt_regs *regs,
		       unsigned long stack_size,
		       int *parent_tidptr, int *child_tidptr)
{
	struct task_struct *task = current;
#ifdef CONFIG_KRG_SCHED_CONFIG
	kerrighed_node_t distant_node;
#else
	static kerrighed_node_t distant_node = -1;
#endif
	struct epm_action remote_clone;
	struct completion vfork;
	pid_t remote_pid = -1;
	int retval;

	if (clone_flags &
	    ~(CSIGNAL |
	      CLONE_CHILD_CLEARTID | CLONE_CHILD_SETTID |
	      CLONE_VFORK | CLONE_SYSVSEM)) {
		/* Unsupported clone flags are requested. Abort */
		retval = -ENOSYS;
		goto out;
	}

	if (!task->sighand->krg_objid || !task->signal->krg_objid ||
	    !task->task_obj || !task->children_obj) {
		retval = -EPERM;
		goto out;
	}

	retval = krg_action_start(task, EPM_REMOTE_CLONE);
	if (retval)
		goto out;

#ifdef CONFIG_KRG_SCHED_CONFIG
	distant_node = new_task_node(task);
#else
	if(distant_node < 0)
		distant_node = kerrighed_node_id;
	distant_node = krgnode_next_online_in_ring(distant_node);
#endif
	if (distant_node < 0 || distant_node == kerrighed_node_id)
		goto out_action_stop;

	remote_clone.type = EPM_REMOTE_CLONE;
	remote_clone.remote_clone.target = distant_node;
	remote_clone.remote_clone.clone_flags = clone_flags;
	remote_clone.remote_clone.stack_start = stack_start;
	remote_clone.remote_clone.stack_size = stack_size;
	remote_clone.remote_clone.from_pid = task->pid;
	remote_clone.remote_clone.from_tgid = task->tgid;
	remote_clone.remote_clone.parent_tidptr = parent_tidptr;
	remote_clone.remote_clone.child_tidptr = child_tidptr;
	if (clone_flags & CLONE_VFORK) {
		init_completion(&vfork);
		remote_clone.remote_clone.vfork = &vfork;
	}

	// return from the syscall with the code: 0
	// TODO: have to fix, in order to handle other kind of return value
	// (other syscall)
	CPUREG_AX(regs) = 0;

	remote_pid = send_task(task, regs, distant_node, &remote_clone);

	if (remote_pid > 0 && (clone_flags & CLONE_VFORK))
		wait_for_completion(&vfork);

 out_action_stop:
	krg_action_stop(task, EPM_REMOTE_CLONE);

 out:
	return remote_pid;

}

static inline struct vfork_done_proxy * vfork_done_proxy_alloc(void)
{
	return kmem_cache_alloc(vfork_done_proxy_cachep, GFP_KERNEL);
}

static inline void vfork_done_proxy_free(struct vfork_done_proxy *proxy)
{
	kmem_cache_free(vfork_done_proxy_cachep, proxy);
}

int export_vfork_done(struct epm_action *action,
		      ghost_t *ghost, struct task_struct *task)
{
	struct vfork_done_proxy proxy;
	int retval = 0;

	switch (action->type) {
	case EPM_MIGRATE:
		if (!task->vfork_done)
			break;
		if (task->remote_vfork_done) {
			proxy = *(struct vfork_done_proxy *) task->vfork_done;
		} else {
			proxy.waiter_vfork_done = task->vfork_done;
			proxy.waiter_node = kerrighed_node_id;
		}
		retval = ghost_write(ghost, &proxy, sizeof(proxy));
		break;
	case EPM_REMOTE_CLONE:
		if (action->remote_clone.clone_flags & CLONE_VFORK) {
			proxy.waiter_vfork_done = action->remote_clone.vfork;
			proxy.waiter_node = kerrighed_node_id;
			retval = ghost_write(ghost, &proxy, sizeof(proxy));
		}
		break;
	default:
		if (task->vfork_done)
			retval = -ENOSYS;
	}

	return retval;
}

static int vfork_done_proxy_install(struct task_struct *task,
				    struct vfork_done_proxy *proxy)
{
	struct vfork_done_proxy *p = vfork_done_proxy_alloc();
	int retval = -ENOMEM;

	if (!p)
		goto out;
	*p = *proxy;
	task->vfork_done = (struct completion *) p;
	task->remote_vfork_done = 1;
	retval = 0;

out:
	return retval;
}

int import_vfork_done(struct epm_action *action,
		      ghost_t *ghost, struct task_struct *task)
{
	struct vfork_done_proxy tmp_proxy;
	int retval = 0;

	switch (action->type) {
	case EPM_MIGRATE:
		if (!task->vfork_done)
			break;

		retval = ghost_read(ghost, &tmp_proxy, sizeof(tmp_proxy));
		if (unlikely(retval))
			goto out;

		if (tmp_proxy.waiter_node == kerrighed_node_id) {
			task->vfork_done = tmp_proxy.waiter_vfork_done;
			task->remote_vfork_done = 0;
			break;
		}

		retval = vfork_done_proxy_install(task, &tmp_proxy);
		break;
	case EPM_REMOTE_CLONE:
		if (action->remote_clone.clone_flags & CLONE_VFORK) {
			retval = ghost_read(ghost, &tmp_proxy, sizeof(tmp_proxy));
			if (unlikely(retval))
				goto out;
			retval = vfork_done_proxy_install(task, &tmp_proxy);
			break;
		}
		/* Fallthrough */
	default:
		task->vfork_done = NULL;
	}

out:
	return retval;
}

void unimport_vfork_done(struct task_struct *task)
{
	struct completion *vfork_done = task->vfork_done;
	if (vfork_done && task->remote_vfork_done)
		vfork_done_proxy_free((struct vfork_done_proxy *) vfork_done);
}

/* Called after having successfuly migrated out task */
void cleanup_vfork_done(struct task_struct *task)
{
	struct completion *vfork_done = task->vfork_done;
	if (vfork_done) {
		task->vfork_done = NULL;
		if (task->remote_vfork_done)
			vfork_done_proxy_free((struct vfork_done_proxy *) vfork_done);
	}
}

static void handle_vfork_done(struct rpc_desc *desc, void *data, size_t size)
{
	struct completion *vfork_done = *(struct completion **) data;

	complete(vfork_done);
}

static void kcb_vfork_done(struct completion *vfork_done)
{
	struct vfork_done_proxy *proxy = (struct vfork_done_proxy *) vfork_done;

	rpc_async(PROC_VFORK_DONE, proxy->waiter_node,
		  &proxy->waiter_vfork_done, sizeof(proxy->waiter_vfork_done));
	vfork_done_proxy_free(proxy);
}

void register_krg_fork_hooks(void)
{
	hook_register(&kh_do_fork, kcb_do_fork);
	hook_register(&kh_vfork_done, kcb_vfork_done);
#ifdef CONFIG_FD_V1
	hook_register(&kh_fd_send_msg, kcb_fd_send_msg);
#endif
}

int proc_krg_fork_start(void)
{
	vfork_done_proxy_cachep =
		kmem_cache_create("vfork_done_proxy",
				  sizeof(struct vfork_done_proxy),
				  0, SLAB_PANIC,
				  NULL, NULL);

	if (rpc_register_void(PROC_VFORK_DONE, handle_vfork_done, 0))
		BUG();

	return 0;
}

void proc_krg_fork_exit(void)
{
}
