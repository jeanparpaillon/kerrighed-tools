/*
 *  Kerrighed/modules/epm/app_checkpoint.c
 *
 *  Copyright (C) 1999-2006 INRIA, Universite de Rennes 1, EDF
 *  Copyright (C) 2007-2008 Matthieu Fertré - INRIA
 */

#include <linux/compile.h>
#include <linux/pid_namespace.h>
#include <linux/syscalls.h>
#include <linux/version.h>
#include <kerrighed/task.h>
#include <kerrighed/children.h>
#include <kerrighed/kerrighed_signal.h>

#include <epm/debug_epm.h>

#define MODULE_NAME "application checkpoint"

#include <rpc/rpcid.h>
#include <rpc/rpc.h>
#include <epm/application/application.h>
#include <epm/application/app_frontier.h>
#include <epm/application/app_shared.h>
#include <epm/application/app_utils.h>
#include <epm/checkpoint.h>
#include <epm/action.h>
#include <epm/epm_internal.h>
#include <kerrighed/sys/checkpoint.h>
#include <ghost/ghost.h>
#include <ctnr/kddm.h>

/*--------------------------------------------------------------------------*/

static inline int save_app_kddm_object(struct app_kddm_object *obj,
				       const credentials_t *user_creds)
{
	ghost_fs_t oldfs;
	ghost_t *ghost;
	int magic = 4342338;
	int r = 0;
	u32 linux_version;

	DEBUG(DBG_APP_CKPT, 1, "Begin - Appid: %ld\n", obj->app_id);

	oldfs = set_ghost_fs(user_creds->uid, user_creds->gid);

	ghost = create_file_ghost(GHOST_WRITE, obj->app_id, obj->chkpt_sn,
				  -1, "global");

	if (IS_ERR(ghost)) {
		r = PTR_ERR(ghost);
		goto exit;
	}

	/* write information about the Linux kernel version */
	linux_version = LINUX_VERSION_CODE;
	r = ghost_write(ghost, &linux_version, sizeof(linux_version));
	if (r)
		goto err_write;
	r = ghost_write_string(ghost, UTS_MACHINE);
	if (r)
		goto err_write;
	r = ghost_write_string(ghost, UTS_VERSION);
	if (r)
		goto err_write;
	r = ghost_write_string(ghost, LINUX_COMPILE_TIME);
	if (r)
		goto err_write;
	r = ghost_write_string(ghost, LINUX_COMPILE_BY);
	if (r)
		goto err_write;
	r = ghost_write_string(ghost, LINUX_COMPILE_HOST);
	if (r)
		goto err_write;
	r = ghost_write_string(ghost, LINUX_COMPILER);
	if (r)
		goto err_write;

	/* write information about the checkpoint itself */
	r = ghost_write(ghost, &obj->app_id, sizeof(obj->app_id));
	if (r)
		goto err_write;
	r = ghost_write(ghost, &obj->chkpt_sn, sizeof(obj->chkpt_sn));
	if (r)
		goto err_write;
	r = ghost_write(ghost, &obj->nodes, sizeof(obj->nodes));
	if (r)
		goto err_write;
	r = ghost_write(ghost, &magic, sizeof(magic));
	if (r)
		goto err_write;

err_write:
	/* End of the really interesting part */
	ghost_close(ghost);

 exit:
	unset_ghost_fs(&oldfs);

	DEBUG(DBG_APP_CKPT, 1, "End - Appid: %ld - r=%d\n", obj->app_id, r);
	return r;
}

static inline int write_task_parent_links(task_state_t *t,
					  ghost_t *ghost,
					  const credentials_t *user_creds)
{
	int r = 0;
	pid_t parent, real_parent, real_parent_tgid;
	pid_t pgrp, session;
	struct children_kddm_object *obj;

	if (!can_be_checkpointed(t->task, user_creds)) {
		r = -EPERM;
		goto error;
	}

	r = ghost_write(ghost, &t->task->pid, sizeof(pid_t));
	if (r)
		goto error;

	r = ghost_write(ghost, &t->task->tgid, sizeof(pid_t));
	if (r)
		goto error;

	obj = kh_parent_children_readlock(t->task, &real_parent_tgid);
	if (obj) {
		r = kh_get_parent(obj, t->task->pid, &parent, &real_parent);
		BUG_ON(r);
		kh_children_unlock(real_parent_tgid);
	} else {
		parent = real_parent = task_pid(child_reaper(current))->nr;
		real_parent_tgid = task_tgid(child_reaper(current))->nr;
	}

	r = ghost_write(ghost, &parent, sizeof(pid_t));
	if (r)
		goto error;
	r = ghost_write(ghost, &real_parent, sizeof(pid_t));
	if (r)
		goto error;
	r = ghost_write(ghost, &real_parent_tgid, sizeof(pid_t));
	if (r)
		goto error;

	pgrp = process_group(t->task);
	r = ghost_write(ghost, &pgrp, sizeof(pid_t));
	if (r)
		goto error;

	session = process_session(t->task);
	r = ghost_write(ghost, &session, sizeof(pid_t));
	if (r)
		goto error;

error:
	DEBUG(DBG_APP_CKPT, 5, "End - Pid: %d, r=%d\n", t->task->pid, r);
	return r;
}

/*
 * Store the _LOCAL_ checkpoint description in a file
 */
static inline int save_local_app(struct app_struct *app, int chkpt_sn,
				 const credentials_t *user_creds)
{
	ghost_fs_t oldfs;
	ghost_t *ghost;
	int r = 0;
	int null = -1;
	task_state_t *t;
	struct list_head *tmp, *element;

	DEBUG(DBG_APP_CKPT, 1, "Begin - Appid: %ld\n", app->app_id);

	oldfs = set_ghost_fs(user_creds->uid, user_creds->gid);

	ghost = create_file_ghost(GHOST_WRITE, app->app_id, chkpt_sn,
				  kerrighed_node_id, "node");

	if (IS_ERR(ghost)) {
		r = PTR_ERR(ghost);
		goto exit;
	}

	/* Here is the really interesting part */
	r = ghost_write(ghost, &kerrighed_node_id, sizeof(kerrighed_node_t));
	if (r)
		goto err_write;

	/* write all the description of the local tasks involved in the
	 * checkpoint
	 * there is no need to lock the application list of processes because
	 * all application processes are already stopped
	 */
	list_for_each_safe(element, tmp, &app->tasks) {
		t = list_entry(element, task_state_t, next_task);
		r = write_task_parent_links(t, ghost, user_creds);
		if (r)
			goto err_write;
	}

	/* end of file marker */
	r = ghost_write(ghost, &null, sizeof(int));
	if (r)
		goto err_write;
	r = ghost_write(ghost, &null, sizeof(int));
	if (r)
		goto err_write;
err_write:
	/* End of the really interesting part */
	ghost_close(ghost);

/* WARNING: if no tasks are finally checkpointable, we should unregister
 *          this node...
 */
 exit:
	unset_ghost_fs(&oldfs);

	DEBUG(DBG_APP_CKPT, 1, "End - Appid: %ld, r=%d\n", app->app_id, r);
	return r;
}


/*
 * "send a request" to checkpoint a local process
 * an ack is send at the end of the checkpoint
 */
static inline void __chkpt_task_req(media_t media,
				    struct task_struct *task,
				    const credentials_t *user_creds)
{
	struct siginfo info;
	int signo;
	int r;

	BUG_ON(!task);

	DEBUG(DBG_APP_CKPT, 1, "Begin - Pid: %d\n", task->pid);

	if (!can_be_checkpointed(task, user_creds)) {
		set_task_chkpt_result(task, -EPERM);
		return;
	}

	signo = KRG_SIG_CHECKPOINT;
	info.si_errno = 0;
	info.si_pid = 0;
	info.si_uid = 0;
	si_media(info) = media;
	si_option(info) = CHKPT_NO_OPTION;

	r = send_kerrighed_signal(signo, &info, task);
	if (r)
		BUG();

	if (!wake_up_process(task)) {
		set_task_chkpt_result(task, -EAGAIN);
	}
	DEBUG(DBG_APP_CKPT, 1, "End - Pid: %d\n", task->pid);
}

/*--------------------------------------------------------------------------*/

static inline int __get_next_chkptsn(long app_id, int original_sn)
{
	char *dirname;
	int error;
	struct nameidata nd;
	int version = original_sn;

	DEBUG(DBG_APP_CKPT, 4, "Begin - AppID: %ld, version: %d\n", app_id,
	      original_sn);
	do {
		version++;
		dirname = get_chkpt_dir(app_id, version);
		error = path_lookup(dirname, 0, &nd);
		if (!error)
			path_release(&nd);
		DEBUG(DBG_APP_CKPT, 5, "lookup -%s- ? %d\n", dirname, error);
		kfree(dirname);
	} while (error != -ENOENT);

	DEBUG(DBG_APP_CKPT, 4, "End - AppID: %ld, version: %d\n", app_id,
	      version);

	return version;
}


/*--------------------------------------------------------------------------*/

/*
 * CHECKPOINT all the processes running _LOCALLY_ which are involved in the
 * checkpoint of an application
 *
 */
static inline int __local_do_chkpt(struct app_struct *app,
				   media_t media, int chkpt_sn,
				   const credentials_t *user_creds)
{
	task_state_t *tsk;
	struct task_struct *tmp = NULL;
	int r;

	BUG_ON(list_empty(&app->tasks));

	DEBUG(DBG_APP_CKPT, 1, "Begin - Appid: %ld\n", app->app_id);

	app->chkpt_sn = chkpt_sn;

	r = save_local_app(app, chkpt_sn, user_creds);
	if (r)
		goto exit;

	/* Checkpoint all local processes involved in the checkpoint */
	init_completion(&app->tasks_chkpted);

	list_for_each_entry(tsk, &app->tasks, next_task) {
		tmp = tsk->task;

		if (tsk->chkpt_result != PCUS_OPERATION_OK) {
			printk("Pid: %d, result: %d\n", tmp->pid, tsk->chkpt_result);
			BUG();
		}

		tsk->chkpt_result = PCUS_CHKPT_IN_PROGRESS;
		BUG_ON(tmp == current);
		DEBUG(DBG_APP_CKPT, 1, " Checkpoint Process %d\n", tmp->pid);
		__chkpt_task_req(media, tmp, user_creds);
	}



	wait_for_completion(&app->tasks_chkpted);
	r = get_local_tasks_chkpt_result(app);
exit:
	DEBUG(DBG_APP_CKPT, 1, "End - Appid: %ld, r=%d\n", app->app_id, r);
	return r;
}

struct checkpoint_request_msg {
	kerrighed_node_t requester;
	long app_id;
	int chkpt_sn;
	media_t media;
	credentials_t creds;
};

static void handle_do_chkpt(struct rpc_desc *desc, void *_msg, size_t size)
{
	struct checkpoint_request_msg *msg = _msg;
	struct app_struct *app = find_local_app(msg->app_id);
	int r;

	DEBUG(DBG_APP_CKPT, 1, "Begin - Appid: %ld\n", msg->app_id);
	BUG_ON(!app);

	r = __local_do_chkpt(app, msg->media, msg->chkpt_sn,
			     &msg->creds);

	r = send_result(desc, r);
	if (r) /* an error as occured on other node */
		goto error;

	r = local_chkpt_shared(desc, app, msg->chkpt_sn, &msg->creds);

	r = send_result(desc, r);

error:
	clear_shared_objects(app);
}

static int global_do_chkpt(struct app_kddm_object *obj,
			   const credentials_t *user_creds, media_t media)
{
	struct rpc_desc *desc;
	struct checkpoint_request_msg msg;
	int r , err_rpc;

	DEBUG(DBG_APP_CKPT, 1, "Begin - Appid: %ld\n", obj->app_id);

	obj->chkpt_sn = __get_next_chkptsn(obj->app_id,
					   obj->chkpt_sn);

	r = save_app_kddm_object(obj, user_creds);
	if (r)
		goto exit;

	/* prepare message */
	msg.requester = kerrighed_node_id;
	msg.app_id = obj->app_id;
	msg.media = media;
	msg.chkpt_sn = obj->chkpt_sn;
	msg.creds = *user_creds;

	desc = rpc_begin_m(APP_DO_CHKPT, &obj->nodes);
	err_rpc = rpc_pack_type(desc, msg);
	if (err_rpc)
		goto err_rpc;

	/* waiting results from the nodes hosting the application */
	r = app_wait_returns_from_nodes(desc, obj->nodes);
	if (r)
		goto err_chkpt;

	err_rpc = rpc_pack_type(desc, r);
	if (err_rpc)
		goto err_rpc;

	r = global_chkpt_shared(desc, obj);

err_chkpt:
	err_rpc = rpc_pack_type(desc, r);
	if (err_rpc)
		goto err_rpc;
exit_rpc:
	rpc_end(desc, 0);

exit:
	DEBUG(DBG_APP_CKPT, 1, "End - Appid: %ld - return=%d\n",
	      obj->app_id, r);
	return r;

err_rpc:
	r = err_rpc;
	rpc_cancel(desc);
	goto exit_rpc;
}


/*--------------------------------------------------------------------------*/

static int _freeze_app(long appid, const credentials_t *user_creds)
{
	int r;
	struct app_kddm_object *obj;

	obj = kddm_grab_object_no_ft(kddm_def_ns, APP_KDDM_ID, appid);
	if (!obj) {
		r = -ESRCH;
		goto exit_kddmput;
	}

	if (obj->state == FROZEN) {
		r = -EPERM;
		goto exit_kddmput;
	}

	r = global_stop(obj, user_creds);
	if (!r)
		obj->state = FROZEN;

exit_kddmput:
	kddm_put_object(kddm_def_ns, APP_KDDM_ID, appid);
	return r;
}

static int __unfreeze_app(struct app_kddm_object *obj, int signal,
			  const credentials_t *user_creds)
{
	int r;

	r = global_continue(obj, 0, user_creds);
	if (r)
		goto err;

	if (signal)
		r = global_kill(obj, signal, user_creds);
err:
	return r;
}

static int _unfreeze_app(long appid, int signal,
			 const credentials_t *user_creds)
{
	int r;
	struct app_kddm_object *obj;

	obj = kddm_grab_object_no_ft(kddm_def_ns, APP_KDDM_ID, appid);
	if (!obj) {
		r = -ESRCH;
		goto exit_kddmput;
	}

	if (obj->state == RUNNING) {
		r = -EPERM;
		goto exit_kddmput;
	}

	r = __unfreeze_app(obj, signal, user_creds);

	if (!r)
		obj->state = RUNNING;

exit_kddmput:
	kddm_put_object(kddm_def_ns, APP_KDDM_ID, appid);
	return r;
}

static int _checkpoint_frozen_app(checkpoint_infos_t *info,
				  const credentials_t *user_creds)
{
	int r;
	struct app_kddm_object *obj;

	obj = kddm_grab_object_no_ft(kddm_def_ns, APP_KDDM_ID, info->app_id);
	if (!obj) {
		r = -ESRCH;
		goto exit_kddmput;
	}

	if (obj->state != FROZEN) {
		r = -EPERM;
		goto exit_kddmput;
	}

	r = global_do_chkpt(obj, user_creds, info->media);

	info->chkpt_sn = obj->chkpt_sn;

exit_kddmput:
	kddm_put_object(kddm_def_ns, APP_KDDM_ID, info->app_id);
	return r;
}

/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/

static inline int create_app_folder(long app_id, int chkpt_sn,
				    const credentials_t *user_creds)
{
	ghost_fs_t oldfs;
	int r;
	oldfs = set_ghost_fs(user_creds->uid, user_creds->gid);
	r = mkdir_chkpt_path(app_id, chkpt_sn);
	unset_ghost_fs(&oldfs);

	return r;
}

long get_appid(const checkpoint_infos_t *info, const credentials_t *user_creds)
{
	long r;

	/* check if user is stupid ;-) */
	if (info->app_id < 0 ||
	    (info->signal < 0 || info->signal >= SIGRTMIN)) {
		r = -EINVAL;
		goto exit;
	}

	switch (info->type) {

	case FROM_PID:
		DEBUG(DBG_APP_CKPT, 1, "FROM PID\n");
		r = get_appid_from_pid(info->app_id, user_creds);
		break;

	case FROM_APPID:
		DEBUG(DBG_APP_CKPT, 1, "FROM APPID\n");
		r = info->app_id;
		break;

	default:
		BUG();
	}

exit:
	return r;
}

int app_freeze(checkpoint_infos_t *info, const credentials_t *user_creds)
{
	int r = -EPERM;
	long app_id = get_appid(info, user_creds);

	if (app_id < 0) {
		r = app_id;
		goto exit;
	}

	/* check that an application does not try to freeze itself */
	if (current->application && current->application->app_id == app_id) {
		r = -EPERM;
		goto exit;
	}

	info->app_id = app_id;

	r = _freeze_app(app_id, user_creds);

exit:
	return r;
}

int app_unfreeze(checkpoint_infos_t *info, const credentials_t *user_creds)
{
	int r = -EPERM;
	long app_id = get_appid(info, user_creds);

	if (app_id < 0) {
		r = app_id;
		goto exit;
	}

	BUG_ON(current->application && current->application->app_id == app_id);
	info->app_id = app_id;

	r = _unfreeze_app(app_id, info->signal, user_creds);
exit:
	return r;
}


int app_chkpt(checkpoint_infos_t *info, const credentials_t *user_creds)
{
       int r = -EPERM;
       long app_id = get_appid(info, user_creds);

       if (app_id < 0) {
               r = app_id;
               goto exit;
       }

       /* check that an application does not try to checkpoint itself */
       if (current->application && current->application->app_id == app_id) {
               r = -EPERM;
               goto exit;
       }

       info->app_id = app_id;

       r = _checkpoint_frozen_app(info, user_creds);
exit:
       return r;
}

void application_checkpoint_rpc_init(void)
{
	rpc_register_void(APP_DO_CHKPT, handle_do_chkpt, 0);
}
