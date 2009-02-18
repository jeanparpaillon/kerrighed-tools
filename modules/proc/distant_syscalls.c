/*
 *  Kerrighed/modules/proc/distant_syscalls.c
 *
 *  Copyright (C) 1999-2006 INRIA, Universite de Rennes 1, EDF
 *  Copyright (C) 2006-2007 Pascal Gallard - Kerlabs, Louis Rilling - Kerlabs
 */

/** Aragorn distant syscall API
 *  @file distant_syscall.c
 *
 *  @author David Margery, Geoffroy Vallee
 */

#include <linux/syscalls.h>
#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/security.h>
#include <kerrighed/krgnodemask.h>
#include <kerrighed/syscalls.h>
#include <kerrighed/capabilities.h>
#include <kerrighed/pid.h>
#ifdef CONFIG_KRG_EPM
#include <kerrighed/krginit.h>
#include <kerrighed/children.h>
#include <kerrighed/signal.h>
#include <kerrighed/sched.h>
#endif

#include "debug_proc.h"

#define MODULE_NAME "syscalls"

#include <tools/syscalls.h>
#include <rpc/rpcid.h>
#include <rpc/rpc.h>
#include <hotplug/hotplug.h>
#include <capability/capability.h>
#include <proc/distant_syscalls.h>
#include <proc/task.h>
#ifdef CONFIG_KRG_EPM
#include <epm/migration_api.h>
#endif


struct change_cap_msg_t {
	pid_t dest_pid;
	struct caller_creds requester_creds;
	krg_cap_t cap;
};

struct kill_info_params {
	int sig;
	struct siginfo info;
	pid_t pid;
	pid_t session;
	uid_t uid;
	uid_t euid;
	int cap_kill;
	u32 secid;
};


static void make_kill_info_param(struct kill_info_params *msg, int sig,
				 struct siginfo *info, pid_t pid)
{
	msg->sig = sig;
	msg->pid = pid;
	msg->session = process_session(current);
	msg->uid = current->uid;
	msg->euid = current->euid;
	msg->cap_kill = capable(CAP_KILL);
	msg->secid = 0;

	security_task_getsecid(current, &msg->secid);
	/* Security ids are not valid cluster-wide right now ... */
	WARN_ON(msg->secid);
	switch ((unsigned long) info) {
	case 0:
		msg->info.si_signo = sig;
		msg->info.si_errno = 0;
		msg->info.si_code = SI_USER;
		msg->info.si_pid = current->pid;
		msg->info.si_uid = current->uid;
		break;
	case 1:
		msg->info.si_signo = sig;
		msg->info.si_errno = 0;
		msg->info.si_code = SI_KERNEL;
		msg->info.si_pid = 0;
		msg->info.si_uid = 0;
		break;
	default:
		copy_siginfo(&msg->info, info);
		break;
	}
}


static int kcb_kill_proc_info(int sig, struct siginfo *info, pid_t pid)
{
	int res = -ESRCH;
	kerrighed_node_t node = KERRIGHED_NODE_ID_NONE;

	DEBUG(DBG_RSYSCALL, 1, "%d sig=%d\n", pid, sig);

	if (!likely(pid & GLOBAL_PID_MASK))
		goto out;

	node = kcb_lock_pid_location(pid);
	if (node != KERRIGHED_NODE_ID_NONE) {
		struct kill_info_params msg;

		make_kill_info_param(&msg, sig, info, pid);

		res = rpc_sync(PROC_KILL_PROC_INFO, node, &msg, sizeof(msg));

		kcb_unlock_pid_location(pid);
	}

 out:
	DEBUG(DBG_RSYSCALL, 1, "done res=%d\n", res);
	return res;
}


static int handle_kill_proc_info(struct rpc_desc* desc, void *msg,
				 size_t size)
{
	struct kill_info_params *params = msg;
	struct task_struct *p;
	int retval = -EINVAL;

	DEBUG(DBG_RSYSCALL, 1, "%d -> %d sig=%d\n",
	      params->info.si_pid, params->pid, params->sig);

	if (!valid_signal(params->sig))
		goto out;

	rcu_read_lock();
	p = find_task_by_pid(params->pid);
	BUG_ON(!p);

	/* Have to do the job of check_kill_permission() twice regarding
	 * euid/uid, CAP_KILL, process session, and security checking
	 */
	retval = -EPERM;
	if (((params->sig != SIGCONT)
	     || (params->session != process_session(p)))
	    && (params->euid ^ p->suid) && (params->euid ^ p->uid)
	    && (params->uid ^ p->suid) && (params->uid ^ p->uid)
	    && !params->cap_kill)
		goto unlock;
	retval = security_task_kill(p, &params->info, params->sig,
				    params->secid);
	if (retval)
		goto unlock;

	retval = kill_pid_info(params->sig, &params->info, task_pid(p));
unlock:
	rcu_read_unlock();

out:
	return retval;
}


struct setscheduler_message {
	pid_t pid;
	int policy;
	struct sched_param param;
	uid_t requester_euid;
	int requester_cap_sys_nice;
};


static int kcb_sched_setscheduler(pid_t pid,
				  int policy, struct sched_param *param)
{
	struct setscheduler_message msg;
	kerrighed_node_t node;
	int retval = -ESRCH;

	DEBUG(DBG_RSYSCALL, 1, "%d\n", pid);

	if (!(pid & GLOBAL_PID_MASK))
		goto out;

	node = kcb_lock_pid_location(pid);
	if (node == KERRIGHED_NODE_ID_NONE)
		goto out;

	msg.pid = pid;
	msg.policy = policy;
	msg.param = *param;
	msg.requester_euid = current->euid;
	msg.requester_cap_sys_nice = capable(CAP_SYS_NICE);

	retval = rpc_sync(PROC_SCHED_SETSCHEDULER, node, &msg, sizeof(msg));

	kcb_unlock_pid_location(pid);

 out:
	DEBUG(DBG_RSYSCALL, 2, "%d retval=%d\n", pid, retval);
	return retval;
}


static int handle_sched_setscheduler(struct rpc_desc* desc, void *_msg,
				     size_t size)
{
	struct setscheduler_message *msg = _msg;
	int orig_euid = current->euid;
	struct task_struct *p;
	int retval;

	BUG_ON(!capable(CAP_SYS_NICE));
	if (!msg->requester_cap_sys_nice) {
		cap_lower(current->cap_effective, CAP_SYS_NICE);
		BUG_ON(capable(CAP_SYS_NICE));
		smp_wmb();
		current->euid = msg->requester_euid;
	}

	rcu_read_lock();

	p = find_task_by_pid(msg->pid);
	BUG_ON(!p);
	retval = sched_setscheduler(p, msg->policy, &msg->param);

	rcu_read_unlock();

	if (!msg->requester_cap_sys_nice) {
		smp_wmb();
		current->euid = orig_euid;
		cap_raise(current->cap_effective, CAP_SYS_NICE);
		BUG_ON(!capable(CAP_SYS_NICE));
	}

	DEBUG(DBG_RSYSCALL, 2, "%d retval=%d\n", msg->pid, retval);
	return retval;
}


static int handle_sched_getscheduler(struct rpc_desc* desc, void *msg,
				     size_t size)
{
	pid_t *pid = msg;

	return sys_sched_getscheduler(*pid);
}


long kcb_sched_getscheduler(pid_t pid)
{
	kerrighed_node_t node;
	int r = -ESRCH;

	node = kcb_lock_pid_location(pid);

	if (node != KERRIGHED_NODE_ID_NONE) {
		r = rpc_sync(PROC_SCHED_GETSCHEDULER, node, &pid, sizeof(pid));
		kcb_unlock_pid_location(pid);
	}

	return r;
}


static int handle_sched_getparam(struct rpc_desc* desc, void *msg,
				 size_t size)
{
	pid_t *pid = msg;
	struct sched_param param;
	int retval;

	/* No recursion is possible since pid is guaranteed to exist on this
	 * node. */
	retval = sys_sched_getparam(*pid, &param);
	DEBUG(DBG_RSYSCALL, 2, "%d retval=%d\n", *pid, retval);

	return retval;
}

int kcb_sched_getparam(pid_t pid, struct sched_param *param)
{
	kerrighed_node_t node;
	int res, r = -ESRCH;

	node = kcb_lock_pid_location(pid);

	if (node != KERRIGHED_NODE_ID_NONE) {
		res = rpc_sync(PROC_SCHED_GETPARAM, node, &pid, sizeof(pid));

		if (res < 0)
			r = res;
		else {
			param->sched_priority = res;
			r = 0;
		}

		kcb_unlock_pid_location(pid);
	}

	DEBUG(DBG_RSYSCALL, 2, "%d retval=%d\n", pid, r);
	return r;
}


#ifdef CONFIG_KRG_EPM

struct group_session {
	pid_t pid;
	pid_t pgid;
	pid_t session;
};

#if 0 /* Disabled */
static int handle_setpgid(struct rpc_desc* desc, void *msg,
			  size_t size)
{
	struct group_session *info = msg;
	int res = 1;
	struct task_struct *p;

	read_lock_irq(&tasklist_lock);

	do_each_task_pid(info->pgid, PIDTYPE_PGID, p) {
		kh_signal_struct_readlock(p->signal->krg_objid);
		if (process_session(p) == info->session) {
			kh_signal_struct_unlock(p->signal->krg_objid);
			goto ok;
		}
		kh_signal_struct_unlock(p->signal->krg_objid);
	}
	while_each_task_pid(pgid, PIDTYPE_PGID, p);

	res = 0;
      ok:
	return res;
}

int kcb_setpgid(pid_t pgid, pid_t session)
{
	struct group_session msg;

	msg.pgid = pgid;
	msg.session = session;

	return generic_cluster_wide_service_call(ARAGORN_SYS_SETPGID,
						 PROC_CHAN,
						 SYNC_MAX_SERVICE, &msg,
						 sizeof(msg));
}
#endif /* 0 */

struct setpgid_message {
	pid_t pid;
	pid_t pgid;
	pid_t parent_tgid;
	int self_setpgid;
};

static int forward_setpgid(kerrighed_node_t node,
			   pid_t pid, pid_t pgid,
			   pid_t parent_tgid)
{
	struct setpgid_message msg;

	msg.pid = pid;
	msg.pgid = pgid;
	msg.parent_tgid = parent_tgid;
	msg.self_setpgid = current->tgid == pid;

	DEBUG(DBG_RSYSCALL, 2, "Calling node %d\n", node);
	return rpc_sync(PROC_FORWARD_SETPGID, node, &msg, sizeof(msg));
}

static int kcb_forward_setpgid(pid_t pid, pid_t pgid)
{
	kerrighed_node_t node;
	struct children_kddm_object *parent_children_obj;
	pid_t parent, real_parent;
	pid_t parent_tgid;
	int retval = -ESRCH;

	DEBUG(DBG_RSYSCALL, 1, "start %d pgid=%d\n", pid, pgid);

	if (!(pid & GLOBAL_PID_MASK))
		goto out;

	if (current->tgid == pid)
		parent_children_obj =
			kh_parent_children_writelock(current, &parent_tgid);
	else {
		/* The forwarded call will neither check that the caller is the
		 * group leader of the parent process, nor check that they are
		 * in the same session.
		 *
		 * Here we check that the group leader of the current process is
		 * the parent of pid. The check for sessions is done after we
		 * are sure that the target process is local. */
		parent_children_obj = kh_children_writelock(current->tgid);
		if (!parent_children_obj) {
			/* If we are the parent of process pid, we can only be
			 * the child_reaper process and as such we are not in
			 * the same session as process pid. */
			kh_children_unlock(current->tgid);
			goto out;
		}
		parent_tgid = current->tgid;
		if (kh_get_parent(parent_children_obj, pid,
				  &parent, &real_parent)
		    || real_parent != current->tgid)
			goto unlock_children;
	}

	node = kcb_lock_pid_location(pid);
	if (node == KERRIGHED_NODE_ID_NONE)
		goto unlock_children;

	retval = forward_setpgid(node, pid, pgid, parent_tgid);

	if (!retval)
		kh_set_child_pgid(parent_children_obj, pid, pgid);

	kcb_unlock_pid_location(pid);
 unlock_children:
	if (parent_children_obj)
		kh_children_unlock(parent_tgid);
 out:
	DEBUG(DBG_RSYSCALL, 2, "end %d pgid=%d retval=%d\n", pid, pgid, retval);
	return retval;
}

static int handle_forward_setpgid(struct rpc_desc* desc, void *_msg, size_t size)
{
	const struct setpgid_message *msg = _msg;
	struct signal_struct *parent_sig = NULL; /* Init to make gcc quiet. */
	struct signal_struct *sig;
	int self_setpgid = msg->self_setpgid;
	int retval = -ESRCH;

	if (!self_setpgid)
		parent_sig = kh_signal_struct_readlock(msg->parent_tgid);
	sig = kh_signal_struct_writelock(msg->pid);
	if (!sig)
		goto unlock_sig;

	if (!self_setpgid) {
		struct task_struct *p;

		retval = -EPERM;
		if (signal_session(sig) != signal_session(parent_sig))
			goto unlock_sig;
		retval = -EACCES;
		rcu_read_lock();
		p = find_task_by_pid(msg->pid);
		rcu_read_unlock();
		/* p won't disappear since its pid location is locked. */
		if (p->did_exec)
			goto unlock_sig;
	}

	retval = sys_setpgid(msg->pid, msg->pgid);

 unlock_sig:
	kh_signal_struct_unlock(msg->pid);
	if (!self_setpgid)
		kh_signal_struct_unlock(msg->parent_tgid);

	return retval;
}

#endif /* CONFIG_KRG_EPM */


static int kcb_getpgid(pid_t pid)
{
	int res = -ESRCH;
	kerrighed_node_t node = KERRIGHED_NODE_ID_NONE;

	BUG_ON(!pid);
	DEBUG(DBG_RSYSCALL, 1, "%d\n", pid);

	if (!likely(pid & GLOBAL_PID_MASK))
		goto out;
	if (pid < 0)
		goto out;

	node = kcb_lock_pid_location(pid);
	if (node != KERRIGHED_NODE_ID_NONE) {
		res = rpc_sync(PROC_GETPGID, node, &pid, sizeof(pid));
		kcb_unlock_pid_location(pid);
	}

 out:
	DEBUG(DBG_RSYSCALL, 1, "done res=%d\n", res);
	return res;
}


static int handle_getpgid(struct rpc_desc* desc, void *msg,
			  size_t size)
{
	pid_t pid = *(pid_t *) msg;

	BUG_ON(!pid);
	DEBUG(DBG_RSYSCALL, 1, "%d\n", pid);
	return sys_getpgid(pid);
}


static int kcb_getsid(pid_t pid)
{
	int res = -ESRCH;
	kerrighed_node_t node = KERRIGHED_NODE_ID_NONE;

	BUG_ON(!pid);
	DEBUG(DBG_RSYSCALL, 1, "%d\n", pid);

	if (!likely(pid & GLOBAL_PID_MASK))
		goto out;
	if (pid < 0)
		goto out;

	node = kcb_lock_pid_location(pid);
	if (node != KERRIGHED_NODE_ID_NONE) {
		DEBUG(DBG_RSYSCALL, 2, "%d on %d\n", pid, node);
		res = rpc_sync(PROC_GETSID, node, &pid, sizeof(pid));

		kcb_unlock_pid_location(pid);
	}

 out:
	DEBUG(DBG_RSYSCALL, 1, "done res=%d\n", res);
	return res;
}


static int handle_getsid(struct rpc_desc* desc, void *msg,
			 size_t size)
{
	pid_t pid = *(pid_t *) msg;

	BUG_ON(!pid);
	return sys_getsid(pid);
}


static int handle_kill_pg_info(struct rpc_desc* desc, void *msg,
			       size_t size)
{
	struct kill_info_params *params = msg;
	int retval;

	DEBUG(DBG_RSYSCALL, 1, "%d sig=%d\n",
	      params->pid, params->sig);

	read_lock(&tasklist_lock);
	retval = __kill_pg_info(params->sig, &params->info, params->pid);
	read_unlock(&tasklist_lock);

	DEBUG(DBG_RSYSCALL, 2, "return %d\n", retval);
	return retval;
}


/* static hashtable_t *global_pgrp_table = NULL; */
/* static spinlock_t global_pgrp_lock = SPIN_LOCK_UNLOCKED; */


void pgrp_is_cluster_wide(pid_t pgrp)
{
/* 	DEBUG(DBG_RSYSCALL, 1, "%d\n", pgrp); */

/* 	spin_lock(&global_pgrp_lock); */
/* 	hashtable_add(global_pgrp_table, pgrp, (void *) ((long) pgrp)); */
/* 	spin_unlock(&global_pgrp_lock); */
}


int is_pgrp_cluster_wide(pid_t pgrp)
{
/* 	int retval; */

/* 	spin_lock(&global_pgrp_lock); */
/* 	retval = hashtable_find(global_pgrp_table, pgrp) != NULL; */
/* 	spin_unlock(&global_pgrp_lock); */

/* 	return retval; */
	return (pgrp & GLOBAL_PID_MASK);
}


static int kcb_kill_pg_info(int sig, struct siginfo *info, pid_t pgid)
{
	struct kill_info_params msg;
	int retval;

	if ((pgid & GLOBAL_PID_MASK) && is_pgrp_cluster_wide(pgid)) {
		struct rpc_desc* desc;
		krgnodemask_t nodes;
		kerrighed_node_t node;

		DEBUG(DBG_RSYSCALL, 1, "%d pgid=%d on all nodes\n", sig, pgid);
		make_kill_info_param(&msg, sig, info, pgid);

		krgnodes_copy(nodes, krgnode_online_map);
		desc = rpc_begin_m(PROC_KILL_PG_INFO, &nodes);

		rpc_pack_type(desc, msg);

		retval = -ESRCH;
		for_each_krgnode_mask(node, nodes) {
			retval = rpc_wait_return_from(desc, node);
			DEBUG(DBG_RSYSCALL, 3, "%d (%d)\n", retval, node);

			if (!retval)
				break;
		}

		rpc_end(desc, 0);
	} else {

		DEBUG(DBG_RSYSCALL, 1, "%d pgid=%d on local node\n",
		      sig, pgid);

		read_lock(&tasklist_lock);
		retval = __kill_pg_info(sig, info, pgid);
		read_unlock(&tasklist_lock);
	}

	DEBUG(DBG_RSYSCALL, 2, "%d pgid=%d retval=%d\n", sig, pgid, retval);
	return retval;
}


struct group_pid {
	pid_t pgrp;
	pid_t ignored_task;
};


#ifdef CONFIG_KRG_EPM

static int handle_will_not_become_orphaned_pgrp(struct rpc_desc* desc,
						void *msg,
						size_t size)
{
#if 0
	struct group_pid *params = msg;
	if (sender != kerrighed_node_id) {
		struct task_struct *p, *son;
		struct distant_task_struct *distant_son;
		read_lock(&tasklist_lock);
		for_each_process(p) {
			if (p->pid != 1 && p->signal->pgrp != params->pgrp) {
				list_for_each_entry(son,
						    &p->children,
						    sibling) {
					if (son->signal->pgrp == params->pgrp
					    && son->state != EXIT_ZOMBIE
					    && son->pid != params->ignored_task
					    && son->signal->session ==
					    p->signal->session)
						goto out_found;
				}
				if (p->krg_task != NULL) {
					DEBUG(DBG_RSYSCALL, 4,
					      "Checking distant sons of %d\n",
					      p->pid);
					for_each_distant_son(p, distant_son) {
						DEBUG(DBG_RSYSCALL, 4,
						      "Checking"
						      " distant sons %d\n",
						      distant_son->pid);
						if (distant_son->pgrp ==
						    params->pgrp
						    && distant_son->state !=
						    EXIT_ZOMBIE
						    && distant_son->pid !=
						    params->ignored_task
						    && distant_son->session ==
						    p->signal->session)
							goto out_found;
					}
				}
			}
		}
		goto out;

	      out_found:
		read_unlock(&tasklist_lock);
		return 1;
	}
      out:
#endif
	return 0;
}


static int kcb_will_become_orphaned_pgrp(pid_t pgrp, pid_t ignored_kid)
{
	/* Disabled until new implementation */
	return 0;

#if 0
	int res = 1;

	DEBUG(DBG_RSYSCALL, 2, "(%d,%d)\n", pgrp, ignored_kid);
	if ((pgrp & GLOBAL_PID_MASK) && is_pgrp_cluster_wide(pgrp)) {
		struct group_pid call_params;
		call_params.pgrp = pgrp;
		if (ignored_kid & GLOBAL_PID_MASK) {
			call_params.ignored_task = ignored_kid;
		} else {
			call_params.ignored_task = 0;
		}

		DEBUG(DBG_RSYSCALL, 2, "Generic cluster wide service call\n");
		res =
		    !generic_cluster_wide_service_call
		    (ARAGORN_WILL_NOT_BECOME_ORPHANED_PGRP, PROC_CHAN,
		     SYNC_MAX_SERVICE, &call_params, sizeof(call_params));
		DEBUG(DBG_RSYSCALL, 2,
		      "Generic cluster wide service call done\n");
	}

	DEBUG(DBG_RSYSCALL, 2, "Returns %d\n", res);
	return res;
#endif
}

#endif /* CONFIG_KRG_EPM */


static int handle_wake_up_process(struct rpc_desc* desc, void *msg,
				  size_t size)
{
	pid_t *pid = msg;
	return krg_wake_up_process(*pid);
}


int distant_wake_up_process(pid_t pid)
{
	int res = -ESRCH;
	kerrighed_node_t node;

	DEBUG(DBG_RSYSCALL, 1, "start %d\n", pid);

	node = kcb_lock_pid_location(pid);
	if (node != KERRIGHED_NODE_ID_NONE) {
		DEBUG(DBG_RSYSCALL, 2, "Calling node %d\n", node);
		res = rpc_sync(PROC_WAKE_UP_PROCESS, node, &pid, sizeof(pid));
		DEBUG(DBG_RSYSCALL, 2, "Called node %d\n", node);

		kcb_unlock_pid_location(pid);
	}

	DEBUG(DBG_RSYSCALL, 1, "done %d with res %d\n", pid, res);
	return res;

}


/** pid is here the cluster wide task_struct identifier */
int krg_wake_up_process(pid_t pid)
{
	int res;
	struct task_struct *sleeping_process;

	read_lock(&tasklist_lock);
	sleeping_process = find_task_by_pid(pid);
	if (sleeping_process) {
		res = wake_up_process(sleeping_process);
		read_unlock(&tasklist_lock);
	} else {
		read_unlock(&tasklist_lock);
		res = distant_wake_up_process(pid);
	}
	return res;
}


struct get_cap_msg_t {
	pid_t dest_pid;
	struct caller_creds requester_creds;
};


static int handle_get_pid_cap(struct rpc_desc* desc, void *_msg,
			      size_t size)
{
	struct get_cap_msg_t *msg = _msg;
	struct task_struct *tsk;
	int err;
	int res = 0;
	int pid_found = 1;

	DEBUG(DBG_RSYSCALL, 1, "start %d\n", msg->dest_pid);
	rcu_read_lock();
	tsk = find_task_by_pid(msg->dest_pid);
	BUG_ON(!tsk);
	/* Should'nt disappear since pid location was locked, but saner
	 * anyway */
	get_task_struct(tsk);
	rcu_read_unlock();

	if (!permissions_ok(tsk, &msg->requester_creds)) {
		pid_found = -EPERM;
		res = -EPERM;
	}

	err = rpc_pack_type(desc, pid_found);
	if (unlikely(err))
		goto out_err;
	if (pid_found < 0)
		goto out;

	task_lock(tsk);

	err = rpc_pack_type(desc, tsk->krg_cap_effective);
	if (unlikely(err))
		goto out_err;
	err = rpc_pack_type(desc, tsk->krg_cap_permitted);
	if (unlikely(err))
		goto out_err;
	err = rpc_pack_type(desc, tsk->krg_cap_inheritable_effective);
	if (unlikely(err))
		goto out_err;
	err = rpc_pack_type(desc, tsk->krg_cap_inheritable_permitted);
	if (unlikely(err))
		goto out_err;

	task_unlock(tsk);

	put_task_struct(tsk);

 out:
	DEBUG(DBG_RSYSCALL, 1, "done %d with res %d\n",
	      msg->dest_pid, res);
	return res;

 out_err:
	task_unlock(tsk);
	put_task_struct(tsk);
	rpc_cancel(desc);
	res = err;
	goto out;
}


int kcb_get_pid_cap(pid_t pid, struct caller_creds *requester_creds,
		    krg_cap_t *requested_cap)
{
	int res = -ESRCH;
	int pid_found;
	kerrighed_node_t node;
	struct get_cap_msg_t msg;
	struct rpc_desc *desc;

	BUG_ON(!requester_creds);

	if (!(pid & GLOBAL_PID_MASK))
		goto out;

	node = kcb_lock_pid_location(pid);

	if (node == KERRIGHED_NODE_ID_NONE)
		goto out;

	msg.dest_pid = pid;
	msg.requester_creds = *requester_creds;

	DEBUG(DBG_RSYSCALL, 1, "Preparing answer from %d\n", node);

	desc = rpc_begin(PROC_GET_PID_CAP, node);
	if (unlikely(!desc)) {
		res = -ENOMEM;
		goto out_unlock;
	}

	DEBUG(DBG_RSYSCALL, 2, "Calling %d\n", node);
	rpc_pack_type(desc, msg);

	rpc_unpack_type(desc, res);
	if (unlikely(res))
		goto out_err_unpack;

	res = rpc_unpack_type(desc, pid_found);
	if (unlikely(res))
		goto out_err_unpack;

	if (pid_found > 0) {
		res = rpc_unpack_type(desc, requested_cap->krg_cap_effective);
		if (unlikely(res))
			goto out_err_unpack;

		res = rpc_unpack_type(desc, requested_cap->krg_cap_permitted);
		if (unlikely(res))
			goto out_err_unpack;

		res = rpc_unpack_type(desc,
				      requested_cap->krg_cap_inheritable_effective);
		if (unlikely(res))
			goto out_err_unpack;

		res = rpc_unpack_type(desc,
				      requested_cap->krg_cap_inheritable_permitted);
		if (unlikely(res))
			goto out_err_unpack;

		res = 0;
	} else
		res = pid_found;

 out_end:
	rpc_end(desc, 0);

 out_unlock:
	kcb_unlock_pid_location(pid);

	if (unlikely(res))
		goto out;

	DEBUG(DBG_RSYSCALL, 1, "returning 0%o, 0%o, 0%o, 0%o\n",
	      requested_cap->krg_cap_permitted,
	      requested_cap->krg_cap_effective,
	      requested_cap->krg_cap_inheritable_permitted,
	      requested_cap->krg_cap_inheritable_effective);
 out:
	DEBUG(DBG_RSYSCALL, 2, "%d retval=%d\n", pid, res);
	return res;

 out_err_unpack:
	rpc_cancel(desc);
	goto out_end;
}


static int handle_set_pid_cap(struct rpc_desc* desc, void *msg,
			      size_t size)
{
	struct change_cap_msg_t *change_cap_msg = msg;
	int res = -ESRCH;
	struct task_struct *p;

	DEBUG(DBG_RSYSCALL, 1, "start %d\n",
	      change_cap_msg->dest_pid);

	read_lock(&tasklist_lock);
	p = find_task_by_pid(change_cap_msg->dest_pid);
	BUG_ON(!p);
	res = krg_set_cap(p, &change_cap_msg->requester_creds,
			  &change_cap_msg->cap);
	read_unlock(&tasklist_lock);

	DEBUG(DBG_RSYSCALL, 1, "done %d with res %d\n",
	      change_cap_msg->dest_pid, res);
	return res;
}


int kcb_set_pid_cap(pid_t pid, struct caller_creds *requester_creds,
		    krg_cap_t * requested_cap)
{
	int res = -ESRCH;
	kerrighed_node_t node;
	struct change_cap_msg_t msg;

	BUG_ON(!requester_creds);

	DEBUG(DBG_RSYSCALL, 1, "start %d\n", pid);

	if (!(pid & GLOBAL_PID_MASK))
		goto out;

	node = kcb_lock_pid_location(pid);
	if (node == KERRIGHED_NODE_ID_NONE)
		goto out;

	msg.dest_pid = pid;
	msg.requester_creds = *requester_creds;
	msg.cap = *requested_cap;
	DEBUG(DBG_RSYSCALL, 2, "Calling node %d\n", node);
	res = rpc_sync(PROC_SET_PID_CAP, node, &msg, sizeof(msg));
	DEBUG(DBG_RSYSCALL, 2, "Called node %d\n", node);

	kcb_unlock_pid_location(pid);

 out:
	DEBUG(DBG_RSYSCALL, 1, "done %d with res %d\n", pid, res);
	return res;
}


#ifdef CONFIG_KRG_EPM

struct migration_request_msg {
	pid_t pid;
	enum migration_scope scope;
	kerrighed_node_t destination_node_id;
	struct caller_creds requester_creds;
};


static int handle_process_migration_request(struct rpc_desc* desc, void *_msg,
					    size_t size)
{
	struct migration_request_msg *msg = _msg;

	return migrate_linux_threads(msg->pid, msg->scope,
				     msg->destination_node_id,
				     &msg->requester_creds);
}


int kcb_migrate_process(pid_t pid,
			enum migration_scope scope,
			kerrighed_node_t destination_node_id,
			struct caller_creds *requester_creds)
{
	struct migration_request_msg msg;
	int res = -ESRCH;
	kerrighed_node_t node;

	DEBUG(DBG_RSYSCALL, 1, "start\n");

	if (!(pid & GLOBAL_PID_MASK)) {
		res = -EPERM;
		goto out;
	}

	node = kh_lock_pid_location(pid);
	if (node == KERRIGHED_NODE_ID_NONE)
		goto out;

	if (node == kerrighed_node_id) {
		res = migrate_linux_threads(pid,
					    scope,
					    destination_node_id,
					    requester_creds);
		goto out_unlock;
	}

	msg.pid = pid;
	msg.scope = scope;
	msg.destination_node_id = destination_node_id;
	msg.requester_creds = *requester_creds;
	DEBUG(DBG_RSYSCALL, 2, "Calling node %d\n", node);
	res = rpc_sync(PROC_REQUEST_MIGRATION, node, &msg, sizeof(msg));
	DEBUG(DBG_RSYSCALL, 2, "Called node %d\n", node);

 out_unlock:
	kh_unlock_pid_location(pid);
 out:
	DEBUG(DBG_RSYSCALL, 1, "done with res %d\n", res);
	return res;

}

#endif /* CONFIG_KRG_EPM */


static void kcb_kill_all(int sig, struct siginfo *info, int *count, int *retval,
			 int tgid)
{
	// Be carefull: what's happen if current is a local process (with a local pid)
	printk("kcb_kill_all: function not implemented\n");
}


void register_distant_syscalls_hooks(void)
{
	//  hook_register(&kh_sys_kill, kcb_sys_kill) ;
	hook_register(&kh_kill_proc_info, kcb_kill_proc_info);
	hook_register(&kh_kill_pg_info, kcb_kill_pg_info);
	hook_register(&kh_kill_all, kcb_kill_all);

	hook_register(&kh_sched_setscheduler, kcb_sched_setscheduler);
	hook_register(&kh_sched_getparam, kcb_sched_getparam);
	hook_register(&kh_sched_getscheduler, kcb_sched_getscheduler);

#ifdef CONFIG_KRG_EPM
	hook_register(&kh_forward_setpgid, kcb_forward_setpgid);
#endif
	hook_register(&kh_getpgid, kcb_getpgid);
	hook_register(&kh_getsid, kcb_getsid);

#ifdef CONFIG_KRG_EPM
	hook_register(&kh_will_become_orphaned_pgrp,
		      kcb_will_become_orphaned_pgrp);
#endif
}


int proc_distant_syscalls_start(void)
{
/* 	global_pgrp_table = hashtable_new(32); */

	rpc_register_int(PROC_KILL_PROC_INFO, handle_kill_proc_info, 0);
	rpc_register_int(PROC_SCHED_SETSCHEDULER, handle_sched_setscheduler, 0);
	rpc_register_int(PROC_SCHED_GETPARAM, handle_sched_getparam, 0);
	rpc_register_int(PROC_SCHED_GETSCHEDULER, handle_sched_getscheduler, 0);
	rpc_register_int(PROC_KILL_PG_INFO, handle_kill_pg_info, 0);

#ifdef CONFIG_KRG_EPM
	//rpc_register_int(ARAGORN_SYS_SETPGID, handle_setpgid);
	rpc_register_int(PROC_FORWARD_SETPGID, handle_forward_setpgid, 0);
#endif /* CONFIG_KRG_EPM */

	rpc_register_int(PROC_GETPGID, handle_getpgid, 0);
	rpc_register_int(PROC_GETSID, handle_getsid, 0);
	
#ifdef CONFIG_KRG_EPM
	rpc_register_int(ARAGORN_WILL_NOT_BECOME_ORPHANED_PGRP,
			 handle_will_not_become_orphaned_pgrp, 0);
#endif /* CONFIG_KRG_EPM */

	rpc_register_int(PROC_WAKE_UP_PROCESS, handle_wake_up_process, 0);
	rpc_register_int(PROC_GET_PID_CAP, handle_get_pid_cap, 0);
	rpc_register_int(PROC_SET_PID_CAP, handle_set_pid_cap, 0);
	
#ifdef CONFIG_KRG_EPM
	rpc_register_int(PROC_REQUEST_MIGRATION,
			 handle_process_migration_request, 0);
#endif /* CONFIG_KRG_EPM */

	return 0;
}


void proc_distant_syscalls_exit(void)
{
/* 	hashtable_free(global_pgrp_table); */
}
