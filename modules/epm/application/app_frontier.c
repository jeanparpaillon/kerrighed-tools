/*
 *  Kerrighed/modules/epm/app_frontier.c
 *
 *  Copyright (C) 1999-2006 INRIA, Universite de Rennes 1, EDF
 *  Copyright (C) 2007-2008 Matthieu Fertré - INRIA
 */

#include <linux/sched.h>
#include <linux/list.h>
#include <kerrighed/sys/types.h>
#include <kerrighed/krginit.h>
#include <kerrighed/pid.h>

#include <epm/debug_epm.h>
#define MODULE_NAME "Application Frontier"

#include <rpc/rpcid.h>
#include <rpc/rpc.h>

#include <epm/application/application.h>
#include <epm/checkpoint.h>


/*--------------------------------------------------------------------------*
 *                                                                          *
 *       USEFULL TO TRAVERSE FILIATION TREE                                 *
 *                                                                          *
 *--------------------------------------------------------------------------*/

static inline struct task_struct *p_cptr(struct task_struct *task)
{
	if (list_empty(&task->children))
		return NULL;

	return list_entry((&(task->children))->next, struct task_struct,
			  sibling);
}


static inline struct task_struct *p_osptr(struct task_struct *task)
{
	return list_entry(task->sibling.next, struct task_struct, sibling);
}


static inline int no_more_brother(struct task_struct *task)
{
	return ((task->sibling.next) == &((task->parent)->children));
}


#define begin_for_each_son_recursive(task,son) \
{					       \
	int gone_up = 0;                       \
	son = task;                            \
	while ( !(son==task && gone_up) ) {    \
		if (!gone_up) {

#define end_for_each_son_recursive(task,son);	       \
		}				       \
		if (p_cptr(son) != NULL && !gone_up) { \
			son = p_cptr(son);	       \
			gone_up = 0;		       \
		} else if (son != task) {	       \
			if ( no_more_brother(son) ) {  \
				son = son->parent;     \
				gone_up = 1;	       \
			} else {		       \
				son = p_osptr(son);    \
				gone_up = 0;	       \
			}			       \
		} else break;			       \
	} son = NULL;				       \
}


/*--------------------------------------------------------------------------*
 *--------------------------------------------------------------------------*/

static inline long __get_appid_from_task(struct task_struct *task,
					 const credentials_t *user_creds)
{
	long r = 0;

	DEBUG(DBG_APP_CKPT, 4, "looking for application of task %d\n",
	      task->pid);

	if (!can_be_checkpointed(task, user_creds)) {
		r = -EPERM;
		goto exit;
	}

	if (!task->application)
		r = create_application(task);

	if (r)
		goto exit;

	BUG_ON(!task->application);
	r = task->application->app_id;
exit:
	return r;
}

struct getappid_request_msg {
	kerrighed_node_t requester;
	pid_t pid;
	credentials_t creds;
};

static inline long __get_appid_from_local_pid(pid_t pid,
					      const credentials_t *user_creds)
{
	struct task_struct * task;

	rcu_read_lock();
	task = find_task_by_pid(pid);
	rcu_read_unlock();
	if (task)
		return __get_appid_from_task(task, user_creds);

	return -ESRCH;
}


long get_appid_from_pid(pid_t pid, const credentials_t *user_creds)
{
	struct rpc_desc *desc;
	kerrighed_node_t n = KERRIGHED_NODE_ID_NONE;
	struct getappid_request_msg msg;
	long app_id;
	int err = 0;

	if (!kh_lock_pid_location) { /* kerrighed is not started... */
		printk("kerrighed is not loaded!\n");
		return -EPERM;
	}

	/* lock the task to be sure it does not exit */
	n = kh_lock_pid_location(pid);
	if (n == KERRIGHED_NODE_ID_NONE)
		return -ESRCH;

	/* the task is local */
	if (n == kerrighed_node_id) {
		app_id =  __get_appid_from_local_pid(pid, user_creds);
		if (app_id < 0)
			err = app_id;
		goto out_unlock;
	}

	err = -ENOMEM;
	msg.requester = kerrighed_node_id;
	msg.pid = pid;
	msg.creds = *user_creds;

	desc = rpc_begin(APP_REMOTE_CHKPT, n);
	if (!desc)
		goto out_unlock;
	err = rpc_pack_type(desc, msg);
	if (err)
		goto err;

	err = rpc_unpack_type(desc, app_id);
	if (err)
		goto err;
 out_end:
	rpc_end(desc, 0);

 out_unlock:
	kh_unlock_pid_location(pid);
	if (err)
		return err;
	return app_id;

 err:
	rpc_cancel(desc);
	goto out_end;
}


static void handle_get_appid_from_pid(struct rpc_desc *desc, void *_msg, size_t size)
{
	struct getappid_request_msg *msg = _msg;
	long app_id;

	app_id = __get_appid_from_local_pid(msg->pid, &msg->creds);
	rpc_pack_type(desc, app_id);
}

void application_frontier_rpc_init(void)
{
	rpc_register_void(APP_REMOTE_CHKPT, handle_get_appid_from_pid, 0);
}
