/** Global /proc/<pid> management
 *  @file proc_info_file.c
 *
 *  Copyright (C) 2001-2006, INRIA, Universite de Rennes 1, EDF.
 *  Copyright (C) 2007, Louis Rilling - Kerlabs.
 */

#include <linux/proc_fs.h>
#include <linux/gfp.h>

#include <kerrighed/sys/types.h>
#include <kerrighed/procfs.h>
#include <kerrighed/pid.h>

#define MODULE_NAME "proc info file"
#include "debug_procfs.h"

#ifndef FILE_NONE
#  if defined(FILE_PROC_INFO_FILE) || defined(FILE_ALL)
#     define DEBUG_THIS_MODULE
#  endif
#endif

#include <tools/debug.h>

#include <rpc/rpcid.h>
#include <rpc/rpc.h>
#include <proc/task.h>

#ifdef DEBUG_THIS_MODULE

#define REQ_ENTRY(name) [name - KRG_PROC_PID_MIN_REQ] = #name,

/* Must be minimal request in rpc/rpcid.h */
#define KRG_PROC_PID_MIN_REQ REQ_PROC_PID_STATUS

static char *__req_name[] = {
	REQ_ENTRY(REQ_PROC_PID_STATUS),
	REQ_ENTRY(REQ_PROC_PID_ENVIRON),
	REQ_ENTRY(REQ_PROC_TGID_STAT),
	REQ_ENTRY(REQ_PROC_PID_STATM),
	REQ_ENTRY(REQ_PROC_PID_CMDLINE),
};

static inline char * req_name(int REQ)
{
	return __req_name[REQ - KRG_PROC_PID_MIN_REQ];
}

#endif /* DEBUG_THIS_MODULE */

#define PROC_BLOCK_SIZE (3*1024)	/* 4K page size but our output routines use some slack for overruns */

static ssize_t krg_proc_info_read(struct file *file, char *buf,
				  size_t count, loff_t * ppos)
{
	struct inode *inode = file->f_dentry->d_inode;
	unsigned long page;
	ssize_t length;
	struct proc_distant_pid_info *task = get_krg_proc_task(inode);

	length = -ESRCH;
	/* TODO: if pid is reused in between, we may think the entry is still
	 * valid! */
	task->prob_node = kcb_lock_pid_location(task->pid);
	if (task->prob_node == KERRIGHED_NODE_ID_NONE)
		/* Task is dead. */
		goto out_no_task;

	if (count > PROC_BLOCK_SIZE)
		count = PROC_BLOCK_SIZE;

	length = -ENOMEM;
	if (!(page = __get_free_page(GFP_KERNEL)))
		goto out;

	length = task->op.proc_read(task, (char *)page);

	if (length >= 0)
		length = simple_read_from_buffer(buf, count, ppos, (char *)page, length);
	free_page(page);
 out:
	kcb_unlock_pid_location(task->pid);
 out_no_task:
	return length;
}

struct file_operations krg_proc_info_file_operations = {
	.read = krg_proc_info_read,
};

struct read_proc_pid_msg {
	pid_t pid;
};

typedef int generic_proc_pid_t(struct task_struct *task, char *buffer);

static int handle_generic_read_proc(struct rpc_desc *desc, void *_msg,
				    int REQ,
				    generic_proc_pid_t generic_proc_pid)
{
	struct read_proc_pid_msg *msg = _msg;
	struct task_struct *tsk;
	unsigned long page;
	int res;
	int err;

	DEBUG(DEBUG_PROC_PID_FILES, 2, "%d %s\n",
	      msg->pid, req_name(REQ));

	rcu_read_lock();
	tsk = find_task_by_pid(msg->pid);
	BUG_ON(!tsk);
	get_task_struct(tsk);
	rcu_read_unlock();

	page = __get_free_page(GFP_KERNEL);
	if (!page) {
		DEBUG(DEBUG_PROC_PID_FILES, 2,
		      "could not allocate memory to get the results\n");
		res = -ENOMEM;
	} else
		res = generic_proc_pid(tsk, (char *)page);

	err = rpc_pack_type(desc, res);
	if (err)
		goto out_err_cancel;
	if (res > 0) {
		err = rpc_pack(desc, 0, (char *)page, res);
		if (err)
			goto out_err_cancel;
	}

 out:
	put_task_struct(tsk);
	if (page)
		free_page(page);
	if (err)
		res = err;

	DEBUG(DEBUG_PROC_PID_FILES, 2, "%d %s res=%d\n",
	      msg->pid, req_name(REQ), res);
	return 0;

 out_err_cancel:
	rpc_cancel(desc);
	goto out;
}

static int generic_krg_proc_pid(struct proc_distant_pid_info *task,
				char *buffer, int REQ)
{
	struct read_proc_pid_msg msg;
	struct rpc_desc *desc;
	int bytes_read;
	int err;

	BUG_ON(task->prob_node == KERRIGHED_NODE_ID_NONE);

	msg.pid = task->pid;

	err = -ENOMEM;
	desc = rpc_begin(REQ, task->prob_node);
	if (!desc)
		goto out_err;

	err = rpc_pack_type(desc, msg);
	if (err)
		goto out_err_cancel;

	err = rpc_unpack_type(desc, bytes_read);
	if (err)
		goto out_err_cancel;
	if (bytes_read > 0)
		err = rpc_unpack(desc, 0, buffer, bytes_read);
	if (err)
		goto out_err_cancel;

	rpc_end(desc, 0);

 out:
	return bytes_read;

 out_err_cancel:
	rpc_cancel(desc);
	rpc_end(desc, 0);
 out_err:
	bytes_read = err;
	goto out;
}

static void handle_read_proc_pid_environ(struct rpc_desc *desc,
					 void *_msg, size_t size)
{
	handle_generic_read_proc(desc, _msg, REQ_PROC_PID_ENVIRON,
				 proc_pid_environ);
}

int krg_proc_pid_environ(struct proc_distant_pid_info *task, char *buffer)
{
	return generic_krg_proc_pid(task, buffer, REQ_PROC_PID_ENVIRON);
}

static void handle_read_proc_pid_status(struct rpc_desc *desc,
					void *_msg, size_t size)
{
	handle_generic_read_proc(desc, _msg, REQ_PROC_PID_STATUS,
				 proc_pid_status);
}

int krg_proc_pid_status(struct proc_distant_pid_info *task, char *buffer)
{
	return generic_krg_proc_pid(task, buffer, REQ_PROC_PID_STATUS);
}

static void handle_read_proc_tgid_stat(struct rpc_desc *desc,
				       void *_msg, size_t size)
{
	handle_generic_read_proc(desc, _msg, REQ_PROC_TGID_STAT,
				 proc_tgid_stat);
}

int krg_proc_tgid_stat(struct proc_distant_pid_info *task, char *buffer)
{
	return generic_krg_proc_pid(task, buffer, REQ_PROC_TGID_STAT);
}

static void handle_read_proc_pid_cmdline(struct rpc_desc *desc,
					 void *_msg, size_t size)
{
	handle_generic_read_proc(desc, _msg, REQ_PROC_PID_CMDLINE,
				 proc_pid_cmdline);
}

int krg_proc_pid_cmdline(struct proc_distant_pid_info *task, char *buffer)
{
	return generic_krg_proc_pid(task, buffer, REQ_PROC_PID_CMDLINE);
}

static void handle_read_proc_pid_statm(struct rpc_desc *desc,
				       void *_msg, size_t size)
{
	handle_generic_read_proc(desc, _msg, REQ_PROC_PID_STATM,
				 proc_pid_statm);
}

int krg_proc_pid_statm(struct proc_distant_pid_info *task, char *buffer)
{
	return generic_krg_proc_pid(task, buffer, REQ_PROC_PID_STATM);
}

void krg_proc_pid_init(void)
{
	rpc_register_void(REQ_PROC_PID_STATUS, handle_read_proc_pid_status, 0);
	rpc_register_void(REQ_PROC_PID_ENVIRON, handle_read_proc_pid_environ, 0);
	rpc_register_void(REQ_PROC_TGID_STAT, handle_read_proc_tgid_stat, 0);
	rpc_register_void(REQ_PROC_PID_STATM, handle_read_proc_pid_statm, 0);
	rpc_register_void(REQ_PROC_PID_CMDLINE, handle_read_proc_pid_cmdline, 0);
}

void krg_proc_pid_finish(void)
{
}
