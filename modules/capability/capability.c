/*
 *  Kerrighed/modules/proc/capability.c
 *
 *  Copyright (C) 1999-2006 INRIA, Universite de Rennes 1, EDF
 *  Copyright (C) 2006-2007 Louis Rilling - Kerlabs
 */

/** writen by David Margery (c) Inria 2004 */

#include <linux/sched.h>
#include <linux/rcupdate.h>
#include <kerrighed/capabilities.h>
#ifdef CONFIG_KRG_EPM
#include <linux/pid_namespace.h>
#include <kerrighed/sched.h>
#include <kerrighed/children.h>
#endif
#include <asm/uaccess.h>

#include "debug_capability.h"

#define MODULE_NAME "capabilities"

#include <tools/krg_syscalls.h>
#include <tools/krg_services.h>
#include <tools/syscalls.h>
#ifdef CONFIG_KRG_PROC
#include <proc/distant_syscalls.h>
#endif


int krg_set_cap(struct task_struct *tsk,
		struct caller_creds *requester_creds, krg_cap_t * requested_cap)
{
	kernel_cap_t tmp_cap;
	int res;
	int i;

	res = -EINVAL;
	if (!requested_cap)
		goto out;

	if (!cap_issubset(requested_cap->krg_cap_effective,
			  requested_cap->krg_cap_permitted)
	    || !cap_issubset(requested_cap->krg_cap_inheritable_permitted,
			     requested_cap->krg_cap_permitted)
	    || !cap_issubset(requested_cap->krg_cap_inheritable_effective,
			     requested_cap->krg_cap_inheritable_permitted))
		goto out;

	res = -ENOSYS;
	tmp_cap = KRG_CAP_SUPPORTED;
	if (!cap_issubset(requested_cap->krg_cap_permitted, tmp_cap))
		goto out;

	res = -EPERM;
	if (!permissions_ok(tsk, requester_creds))
		goto out;

	task_lock(tsk);

	if (!cap_raised(tsk->krg_cap_effective, CAP_CHANGE_KERRIGHED_CAP))
		goto out_unlock;

	res = -EBUSY;
	for (i = 0; i < CAP_SIZE; i++)
		if (atomic_read(&tsk->krg_cap_used[i])
		    && !cap_raised(requested_cap->krg_cap_effective, i))
			goto out_unlock;

	tmp_cap = cap_intersect(tsk->krg_cap_permitted,
				requested_cap->krg_cap_permitted);
	tsk->krg_cap_permitted = tmp_cap;
	tmp_cap = cap_intersect(tsk->krg_cap_permitted,
				requested_cap->krg_cap_effective);
	tsk->krg_cap_effective = tmp_cap;
	tmp_cap = cap_intersect(tsk->krg_cap_permitted,
				requested_cap->krg_cap_inheritable_effective);
	tsk->krg_cap_inheritable_effective = tmp_cap;
	tmp_cap = cap_intersect(tsk->krg_cap_permitted,
				requested_cap->krg_cap_inheritable_permitted);
	tsk->krg_cap_inheritable_permitted = tmp_cap;

	res = 0;

out_unlock:
	task_unlock(tsk);

out:
	return res;
}


static int krg_set_father_cap(struct task_struct *tsk,
			      struct caller_creds *requester_creds,
			      krg_cap_t * requested_cap)
{
	int retval = 0;

	read_lock(&tasklist_lock);
#ifdef CONFIG_KRG_EPM
	if (tsk->parent != baby_sitter) {
#endif
		retval = krg_set_cap(tsk->parent,
				     requester_creds, requested_cap);
		read_unlock(&tasklist_lock);
#ifdef CONFIG_KRG_EPM
	} else {
		struct children_kddm_object *parent_children_obj;
		pid_t real_parent_tgid;
		pid_t parent_pid, real_parent_pid;
		int retval;

		read_unlock(&tasklist_lock);

		parent_children_obj =
			kh_parent_children_readlock(tsk, &real_parent_tgid);
		if (!parent_children_obj)
			/* Parent is init. Do not change init's capabilities! */
			return -EPERM;
		kh_get_parent(parent_children_obj, tsk->pid,
			      &parent_pid, &real_parent_pid);
		retval = kcb_set_pid_cap(real_parent_pid,
					 requester_creds, requested_cap);
		kh_children_unlock(real_parent_tgid);
	}
#endif

	return retval;
}


static int krg_set_pid_cap(pid_t pid,
			   struct caller_creds *requester_creds,
			   krg_cap_t * requested_cap)
{
	struct task_struct *tsk;
	int retval = -ESRCH;

	rcu_read_lock();
	tsk = find_task_by_pid(pid);
	if (tsk)
		retval = krg_set_cap(tsk, requester_creds, requested_cap);
	rcu_read_unlock();
#ifdef CONFIG_KRG_PROC
	if (!tsk)
		retval = kcb_set_pid_cap(pid, requester_creds, requested_cap);
#endif

	return retval;
}


static int krg_get_cap(struct task_struct *tsk,
		       struct caller_creds *requester_creds,
		       krg_cap_t * resulting_cap)
{
	int res;
	DEBUG(DBG_CAP, 1, "(%p, %p)\n", tsk, resulting_cap);

	task_lock(tsk);

	if (resulting_cap && permissions_ok(tsk, requester_creds)) {
		resulting_cap->krg_cap_permitted = tsk->krg_cap_permitted;
		resulting_cap->krg_cap_effective = tsk->krg_cap_effective;
		resulting_cap->krg_cap_inheritable_effective =
		    tsk->krg_cap_inheritable_effective;
		resulting_cap->krg_cap_inheritable_permitted =
		    tsk->krg_cap_inheritable_permitted;
		DEBUG(DBG_CAP, 2,
		      "Capabilities of process %d are {%d,%d,%d,%d}\n",
		      tsk->pid, tsk->krg_cap_permitted, tsk->krg_cap_effective,
		      tsk->krg_cap_inheritable_permitted,
		      tsk->krg_cap_inheritable_effective);
		res = 0;
	} else {
		DEBUG(DBG_CAP, 2,
		      "Could not show capabilities"
		      " of process %d which are {%d,%d,%d,%d}\n",
		      tsk->pid, tsk->krg_cap_permitted, tsk->krg_cap_effective,
		      tsk->krg_cap_inheritable_permitted,
		      tsk->krg_cap_inheritable_effective);
		res = -EPERM;
	}

	task_unlock(tsk);

	return res;
}


static int krg_get_father_cap(struct task_struct *son,
			      struct caller_creds *requester_creds,
			      krg_cap_t * resulting_cap)
{
	int retval = 0;

	read_lock(&tasklist_lock);
#ifdef CONFIG_KRG_EPM
	if (son->parent != baby_sitter) {
#endif
		retval = krg_get_cap(son->parent,
				     requester_creds, resulting_cap);
		read_unlock(&tasklist_lock);
#ifdef CONFIG_KRG_EPM
	} else {
		struct children_kddm_object *parent_children_obj;
		pid_t real_parent_tgid;
		pid_t parent_pid, real_parent_pid;
		int retval;

		read_unlock(&tasklist_lock);

		parent_children_obj =
			kh_parent_children_readlock(son, &real_parent_tgid);
		if (!parent_children_obj)
			/* Parent is init. */
			return krg_get_cap(child_reaper(son),
					   requester_creds, resulting_cap);
		kh_get_parent(parent_children_obj, son->pid,
			      &parent_pid, &real_parent_pid);
		retval = kcb_get_pid_cap(parent_pid,
					 requester_creds, resulting_cap);
		kh_children_unlock(real_parent_tgid);
	}
#endif

	return retval;
}


static int krg_get_pid_cap(pid_t pid,
			   struct caller_creds *requester_creds,
			   krg_cap_t * resulting_cap)
{
	struct task_struct *tsk;
	int retval = -ESRCH;

	rcu_read_lock();
	tsk = find_task_by_pid(pid);
	if (tsk)
		retval = krg_get_cap(tsk, requester_creds, resulting_cap);
	rcu_read_unlock();
#ifdef CONFIG_KRG_PROC
	if (!tsk)
		retval = kcb_get_pid_cap(pid, requester_creds, resulting_cap);
#endif

	return retval;
}


/* Kerrighed syscalls interface */

int proc_set_pid_cap(void *arg)
{
	int r;
	struct krg_cap_pid_desc desc;
	krg_cap_t krg_capabilities;
	struct caller_creds requester_creds;
	requester_creds.caller_uid = current->uid;
	requester_creds.caller_euid = current->euid;

	BUG_ON(sizeof(int) != sizeof(kernel_cap_t));

	if (copy_from_user(&desc, arg, sizeof(desc)))
		r = -EFAULT;
	else {
		if (copy_from_user(&krg_capabilities, desc.caps,
				   sizeof(krg_capabilities)))
			r = -EFAULT;
		else
			r = krg_set_pid_cap(desc.pid, &requester_creds,
					    &krg_capabilities);
	}
	return r;
}


int proc_set_father_cap(void *arg)
{
	int r;
	krg_cap_t krg_capabilities;
	struct caller_creds requester_creds;
	requester_creds.caller_uid = current->uid;
	requester_creds.caller_euid = current->euid;

	BUG_ON(sizeof(int) != sizeof(kernel_cap_t));

	if (copy_from_user(&krg_capabilities, arg, sizeof(krg_capabilities)))
		r = -EFAULT;
	else
		r = krg_set_father_cap(current, &requester_creds,
				       &krg_capabilities);

	return r;
}


int proc_set_cap(void *arg)
{
	int r;
	krg_cap_t krg_capabilities;
	struct caller_creds requester_creds;
	requester_creds.caller_uid = current->uid;
	requester_creds.caller_euid = current->euid;

	BUG_ON(sizeof(int) != sizeof(kernel_cap_t));

	if (copy_from_user(&krg_capabilities, arg, sizeof(krg_capabilities)))
		r = -EFAULT;
	else
		r = krg_set_cap(current, &requester_creds, &krg_capabilities);

	return r;
}


int proc_get_cap(void *arg)
{
	int r;
	krg_cap_t krg_capabilities;
	struct caller_creds requester_creds;
	requester_creds.caller_uid = current->uid;
	requester_creds.caller_euid = current->euid;

	BUG_ON(sizeof(int) != sizeof(kernel_cap_t));

	r = krg_get_cap(current, &requester_creds, &krg_capabilities);

	if (!r) {
		if (copy_to_user
		    (arg, &krg_capabilities, sizeof(krg_capabilities)))
			r = -EFAULT;
	}
	return r;
}


int proc_get_father_cap(void *arg)
{
	int r;
	krg_cap_t krg_capabilities;
	struct caller_creds requester_creds;
	requester_creds.caller_uid = current->uid;
	requester_creds.caller_euid = current->euid;

	BUG_ON(sizeof(int) != sizeof(kernel_cap_t));

	r = krg_get_father_cap(current, &requester_creds, &krg_capabilities);

	DEBUG(DBG_CAP, 2,
	      "Capabilities of father of process %d are {%d,%d,%d,%d}\n",
	      current->pid, krg_capabilities.krg_cap_permitted,
	      krg_capabilities.krg_cap_effective,
	      krg_capabilities.krg_cap_inheritable_permitted,
	      krg_capabilities.krg_cap_inheritable_effective);

	if (!r) {
		if (copy_to_user
		    (arg, &krg_capabilities, sizeof(krg_capabilities)))
			r = -EFAULT;
	}
	return r;
}


int proc_get_pid_cap(void *arg)
{
	int r;
	struct krg_cap_pid_desc desc;
	krg_cap_t krg_capabilities;
	struct caller_creds requester_creds;
	requester_creds.caller_uid = current->uid;
	requester_creds.caller_euid = current->euid;

	BUG_ON(sizeof(int) != sizeof(kernel_cap_t));
	BUG_ON(sizeof(int) != sizeof(pid_t));

	if (copy_from_user(&desc, arg, sizeof(desc))) {
		DEBUG(DBG_CAP, 2,
		      "Could not get the descriptor"
		      " to find the concerned pid\n");
		r = -EFAULT;
	} else {
		r = krg_get_pid_cap(desc.pid, &requester_creds,
				    &krg_capabilities);

		DEBUG(DBG_CAP, 2,
		      "Capabilities of process %d are {%d,%d,%d,%d}\n",
		      desc.pid, krg_capabilities.krg_cap_permitted,
		      krg_capabilities.krg_cap_effective,
		      krg_capabilities.krg_cap_inheritable_permitted,
		      krg_capabilities.krg_cap_inheritable_effective);

		if (!r) {
			if (copy_to_user
			    (desc.caps, &krg_capabilities,
			     sizeof(krg_capabilities))) {
				DEBUG(DBG_CAP, 2,
				      "Could not copy the result back\n");
				r = -EFAULT;
			}
		}
	}
	return r;
}

int proc_get_supported_cap(void __user *arg)
{
	int __user *set = arg;
	return put_user(cap_t(KRG_CAP_SUPPORTED), set);
}

int init_krg_cap(void)
{
	int r;

	r = register_proc_service(KSYS_SET_CAP, proc_set_cap);
	if (r != 0)
		goto out;

	r = register_proc_service(KSYS_GET_CAP, proc_get_cap);
	if (r != 0)
		goto unreg_set_cap;

	r = register_proc_service(KSYS_SET_FATHER_CAP, proc_set_father_cap);
	if (r != 0)
		goto unreg_get_cap;

	r = register_proc_service(KSYS_GET_FATHER_CAP, proc_get_father_cap);
	if (r != 0)
		goto unreg_set_father_cap;

	r = register_proc_service(KSYS_SET_PID_CAP, proc_set_pid_cap);
	if (r != 0)
		goto unreg_get_father_cap;

	r = register_proc_service(KSYS_GET_PID_CAP, proc_get_pid_cap);
	if (r != 0)
		goto unreg_set_pid_cap;

	r = register_proc_service(KSYS_GET_SUPPORTED_CAP,
				  proc_get_supported_cap);
	if (r != 0)
		goto unreg_get_pid_cap;
 out:
	return r;

 unreg_get_pid_cap:
	unregister_proc_service(KSYS_GET_PID_CAP);
 unreg_set_pid_cap:
	unregister_proc_service(KSYS_SET_PID_CAP);
 unreg_get_father_cap:
	unregister_proc_service(KSYS_GET_FATHER_CAP);
 unreg_set_father_cap:
	unregister_proc_service(KSYS_SET_FATHER_CAP);
 unreg_get_cap:
	unregister_proc_service(KSYS_GET_CAP);
 unreg_set_cap:
	unregister_proc_service(KSYS_SET_CAP);
	goto out;
}


void cleanup_krg_cap(void)
{
	unregister_proc_service(KSYS_GET_SUPPORTED_CAP);
	unregister_proc_service(KSYS_GET_PID_CAP);
	unregister_proc_service(KSYS_SET_PID_CAP);
	unregister_proc_service(KSYS_GET_FATHER_CAP);
	unregister_proc_service(KSYS_SET_FATHER_CAP);
	unregister_proc_service(KSYS_GET_CAP);
	unregister_proc_service(KSYS_SET_CAP);

	return;
}
