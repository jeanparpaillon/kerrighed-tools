/** Global cluster information management.
 *  @file proc_pid_info.c
 *
 *  Copyright (C) 2001-2006, INRIA, Universite de Rennes 1, EDF.
 *  Copyright (C) 2007, Louis Rilling - Kerlabs.
 */

#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/stat.h>
#include <linux/sched.h>
#include <kerrighed/procfs.h>
#include <kerrighed/pid.h>
#include <kerrighed/task.h>
#include <kerrighed/krginit.h>

#define MODULE_NAME "proc pid Info "
#include "debug_procfs.h"

#ifndef FILE_NONE
#  if defined(FILE_PROC_PID_INFO) || defined(FILE_ALL)
#     define DEBUG_THIS_MODULE
#  endif
#endif

#include <tools/debug.h>

#include <hotplug/hotplug.h>

#include <rpc/rpcid.h>
#include <rpc/rpc.h>
#include <proc/task.h>
#include <proc/pid.h>
#include "proc_pid_info.h"
#include "proc_fd_info.h"
#include "proc_pid_link_info.h"
#include "proc_info_file.h"
#include "proc_pid_misc.h"

/* Worst case buffer size needed for holding an integer. */
#define PROC_NUMBUF 13

/* Custom types for remote pid entries, which differ from local pid entries in
 * one argument type in proc_read because we cannot provide task structs as
 * arguments. */

struct krg_pid_entry {
	int len;
	char *name;
	mode_t mode;
	struct inode_operations *iop;
	struct file_operations *fop;
	union proc_distant_op op;
};

#define NOD(NAME, MODE, IOP, FOP, OP) {			\
	.len  = sizeof(NAME) - 1,			\
	.name = (NAME),					\
	.mode = MODE,					\
	.iop  = IOP,					\
	.fop  = FOP,					\
	.op   = OP,					\
}

#define DIR(NAME, MODE, OTYPE)							\
	NOD(NAME, (S_IFDIR|(MODE)),						\
		&krg_proc_##OTYPE##_inode_operations,				\
		&krg_proc_##OTYPE##_operations,					\
		{} )
#define LNK(NAME, OTYPE)					\
	NOD(NAME, (S_IFLNK|S_IRWXUGO),				\
		&krg_proc_pid_link_inode_operations, NULL,	\
		{ .proc_get_link = &krg_proc_##OTYPE##_link } )
#define REG(NAME, MODE, OTYPE)				\
	NOD(NAME, (S_IFREG|(MODE)), NULL,		\
		&krg_proc_##OTYPE##_operations, {})
#define INF(NAME, MODE, OTYPE)				\
	NOD(NAME, (S_IFREG|(MODE)), 			\
		NULL, &krg_proc_info_file_operations,	\
		{ .proc_read = &krg_proc_##OTYPE } )

static struct inode *krg_proc_pid_make_inode(struct super_block *sb,
					     struct proc_distant_pid_info *task)
{
	struct inode *inode;
	struct proc_distant_pid_info *ei;

	/* We need a new inode */

	inode = new_inode(sb);
	if (!inode)
		goto out;

	/* Common stuff */
	inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME;
	inode->i_op = &proc_def_inode_operations;

	ei = get_krg_proc_task(inode);
	kh_task_get(task->task_obj);
	*ei = *task;

	inode->i_uid = 0;
	inode->i_gid = 0;
	if (task->dumpable) {
		inode->i_uid = task->euid;
		inode->i_gid = task->egid;
	}

 out:
	return inode;
}

static int krg_pid_getattr(struct vfsmount *mnt, struct dentry *dentry,
			   struct kstat *stat)
{
	struct inode *inode = dentry->d_inode;
	struct proc_distant_pid_info *task;
	generic_fillattr(inode, stat);

	stat->uid = 0;
	stat->gid = 0;
	task = get_krg_proc_task(inode);
	if (kh_task_alive(task->task_obj)) {
		if ((inode->i_mode == (S_IFDIR|S_IRUGO|S_IXUGO)) ||
		    task->dumpable) {
			stat->uid = task->euid;
			stat->gid = task->egid;
		}
	}

	return 0;
}

static int krg_pid_revalidate(struct dentry *dentry, struct nameidata *nd)
{
	//we need to check the pid still exist in the system
	struct inode *inode = dentry->d_inode;
	struct proc_distant_pid_info *ei = get_krg_proc_task(inode);
	struct task_kddm_object *obj;
	long state = EXIT_DEAD;

	/* Optimization: avoid doing kcb_task_readlock when it is obviously
	 * useless. */
	if (!kh_task_alive(ei->task_obj))
		goto drop;
	/* If pid is reused in between, the former task_obj field is dead. */
	obj = kcb_task_readlock(ei->pid);
	if (!kh_task_alive(ei->task_obj) || !obj)
		goto unlock;

	BUG_ON(obj != ei->task_obj);
	state = obj->exit_state;
	if (obj->node == kerrighed_node_id)
		/* The task is probably not dead, but we want the dentry
		 * to be regenerated with vanilla procfs operations. */
		state = EXIT_DEAD;
	if (state != EXIT_DEAD) {
		ei->dumpable = obj->dumpable;
		if ((inode->i_mode == (S_IFDIR|S_IRUGO|S_IXUGO)) ||
		    obj->dumpable) {
			inode->i_uid = obj->euid;
			inode->i_gid = obj->egid;
		} else {
			inode->i_uid = 0;
			inode->i_gid = 0;
		}
		inode->i_mode &= ~(S_ISUID | S_ISGID);
	}

 unlock:
	kcb_task_unlock(ei->pid);

	if (state != EXIT_DEAD)
		return 1;
 drop:
	d_drop(dentry);
	return 0;
}

static int krg_pid_delete_dentry(struct dentry *dentry)
{
	struct proc_distant_pid_info *ei = get_krg_proc_task(dentry->d_inode);

	/* If the task is local, we want the dentry to be regenerated with
	 * vanilla procfs operations. */
	if (!kh_task_alive(ei->task_obj)
	    || ei->task_obj->node == kerrighed_node_id)
		return 1;

	return 0;
}

static struct dentry_operations krg_pid_dentry_operations = {
	.d_revalidate = krg_pid_revalidate,
	.d_delete = krg_pid_delete_dentry,
};

typedef struct dentry *instantiate_t(struct inode *, struct dentry *,
				     struct proc_distant_pid_info *, void *);

static int krg_proc_fill_cache(struct file *filp,
			       void *dirent, filldir_t filldir,
			       char *name, int len,
			       instantiate_t instantiate,
			       struct proc_distant_pid_info *task, void *ptr)
{
	struct dentry *child, *dir = filp->f_path.dentry;
	struct inode *inode;
	struct qstr qname;
	ino_t ino = 0;
	unsigned type = DT_UNKNOWN;

	qname.name = name;
	qname.len  = len;
	qname.hash = full_name_hash(name, len);

	child = d_lookup(dir, &qname);
	if (!child) {
		struct dentry *new;
		new = d_alloc(dir, &qname);
		if (new) {
			child = instantiate(dir->d_inode, new, task, ptr);
			if (child)
				dput(new);
			else
				child = new;
		}
	}
	if (!child || IS_ERR(child) || !child->d_inode)
		goto end_instantiate;
	inode = child->d_inode;
	if (inode) {
		ino = inode->i_ino;
		type = inode->i_mode >> 12;
	}
	dput(child);
end_instantiate:
	if (!ino)
		ino = find_inode_number(dir, &qname);
	if (!ino)
		ino = 1;
	return filldir(dirent, name, len, filp->f_pos, ino, type);
}

static struct dentry *krg_proc_pident_instantiate(struct inode *dir,
						  struct dentry *dentry,
						  struct proc_distant_pid_info *task,
						  void *ptr)
{
	struct krg_pid_entry *p = ptr;
	struct inode *inode;
	struct proc_distant_pid_info *new_info;
	struct dentry *error = ERR_PTR(-EINVAL);

	inode = krg_proc_pid_make_inode(dir->i_sb, task);
	if (!inode)
		goto out;

	new_info = get_krg_proc_task(inode);
	inode->i_mode = p->mode;
	if (S_ISDIR(inode->i_mode))
		inode->i_nlink = 2;	/* Use getattr to fix if necessary */
	if (p->iop)
		inode->i_op = p->iop;
	if (p->fop)
		inode->i_fop = p->fop;
	new_info->op = p->op;
	dentry->d_op = &krg_pid_dentry_operations;
	d_add(dentry, inode);
	error = NULL;
out:
	return error;
}

struct dentry *krg_proc_pident_lookup(struct inode *dir,
				      struct dentry *dentry,
				      struct krg_pid_entry *ents,
				      unsigned int nents)
{
	struct inode *inode;
	struct dentry *error;
	struct proc_distant_pid_info *task = get_krg_proc_task(dir);
	struct krg_pid_entry *p, *last;

	error = ERR_PTR(-ENOENT);
	inode = NULL;

	if (!kh_task_alive(task->task_obj))
		goto out;

	/*
	 * Yes, it does not scale. And it should not. Don't add
	 * new entries into /proc/<tgid>/ without very good reasons.
	 */
	last = &ents[nents - 1];
	for (p = ents; p <= last; p++) {
		if (p->len != dentry->d_name.len)
			continue;
		if (!memcmp(dentry->d_name.name, p->name, p->len))
			break;
	}
	if (p > last)
		goto out;

	error = krg_proc_pident_instantiate(dir, dentry, task, p);
out:
	return error;
}

static int krg_proc_pident_fill_cache(struct file *filp,
				      void *dirent, filldir_t filldir,
				      struct proc_distant_pid_info *task,
				      struct krg_pid_entry *p)
{
	return krg_proc_fill_cache(filp, dirent, filldir, p->name, p->len,
				   krg_proc_pident_instantiate, task, p);
}

static int krg_proc_pident_readdir(struct file *filp,
				   void *dirent, filldir_t filldir,
				   struct krg_pid_entry *ents,
				   unsigned int nents)
{
	int i;
	int pid;
	struct dentry *dentry = filp->f_path.dentry;
	struct inode *inode = dentry->d_inode;
	struct proc_distant_pid_info *task = get_krg_proc_task(inode);
	struct krg_pid_entry *p, *last;
	ino_t ino;
	int ret;

	ret = -ENOENT;
	if (!kh_task_alive(task->task_obj))
		goto out;

	ret = 0;
	pid = task->pid;
	i = filp->f_pos;
	switch (i) {
	case 0:
		ino = inode->i_ino;
		if (filldir(dirent, ".", 1, i, ino, DT_DIR) < 0)
			goto out;
		i++;
		filp->f_pos++;
		/* fall through */
	case 1:
		ino = parent_ino(dentry);
		if (filldir(dirent, "..", 2, i, ino, DT_DIR) < 0)
			goto out;
		i++;
		filp->f_pos++;
		/* fall through */
	default:
		i -= 2;
		if (i >= nents) {
			ret = 1;
			goto out;
		}
		p = ents + i;
		last = &ents[nents - 1];
		while (p <= last) {
			if (krg_proc_pident_fill_cache(filp, dirent, filldir, task, p) < 0)
				goto out;
			filp->f_pos++;
			p++;
		}
	}

	ret = 1;
out:
	return ret;
}

/* Unsupported entries are commented out */
static struct krg_pid_entry krg_tgid_base_stuff[] = {
/* 	DIR("task",       S_IRUGO|S_IXUGO, task), */
	DIR("fd",         S_IRUSR|S_IXUSR, fd),
	INF("environ",    S_IRUSR, pid_environ),
/* 	INF("auxv",       S_IRUSR, pid_auxv), */
	INF("status",     S_IRUGO, pid_status),
	INF("cmdline",    S_IRUGO, pid_cmdline),
	INF("stat",       S_IRUGO, tgid_stat),
	INF("statm",      S_IRUGO, pid_statm),
/* 	REG("maps",       S_IRUGO, maps), */
/* #ifdef CONFIG_NUMA */
/* 	REG("numa_maps",  S_IRUGO, numa_maps), */
/* #endif */
/* 	REG("mem",        S_IRUSR|S_IWUSR, mem), */
/* #ifdef CONFIG_SECCOMP */
/* 	REG("seccomp",    S_IRUSR|S_IWUSR, seccomp), */
/* #endif */
/* 	LNK("cwd",        cwd), */
/* 	LNK("root",       root), */
/* 	LNK("exe",        exe), */
/* 	REG("mounts",     S_IRUGO, mounts), */
/* 	REG("mountstats", S_IRUSR, mountstats), */
/* #ifdef CONFIG_MMU */
/* 	REG("smaps",      S_IRUGO, smaps), */
/* #endif */
/* #ifdef CONFIG_SECURITY */
/* 	DIR("attr",       S_IRUGO|S_IXUGO, attr_dir), */
/* #endif */
/* #ifdef CONFIG_KALLSYMS */
/* 	INF("wchan",      S_IRUGO, pid_wchan), */
/* #endif */
/* #ifdef CONFIG_SCHEDSTATS */
/* 	INF("schedstat",  S_IRUGO, pid_schedstat), */
/* #endif */
/* #ifdef CONFIG_CPUSETS */
/* 	REG("cpuset",     S_IRUGO, cpuset), */
/* #endif */
/* 	INF("oom_score",  S_IRUGO, oom_score), */
/* 	REG("oom_adj",    S_IRUGO|S_IWUSR, oom_adjust), */
/* #ifdef CONFIG_AUDITSYSCALL */
/* 	REG("loginuid",   S_IWUSR|S_IRUGO, loginuid), */
/* #endif */
/* #ifdef CONFIG_FAULT_INJECTION */
/* 	REG("make-it-fail", S_IRUGO|S_IWUSR, fault_inject), */
/* #endif */
/* #ifdef CONFIG_TASK_IO_ACCOUNTING */
/* 	INF("io",	S_IRUGO, pid_io_accounting), */
/* #endif */
};

static int krg_proc_tgid_base_readdir(struct file *filp,
				      void *dirent, filldir_t filldir)
{
	return krg_proc_pident_readdir(filp, dirent, filldir,
				       krg_tgid_base_stuff,
				       ARRAY_SIZE(krg_tgid_base_stuff));
}

static struct file_operations krg_proc_tgid_base_operations = {
	.read		= generic_read_dir,
	.readdir	= krg_proc_tgid_base_readdir,
};

static struct dentry *krg_proc_tgid_base_lookup(struct inode *dir,
						struct dentry *dentry,
						struct nameidata *nd)
{
	return krg_proc_pident_lookup(dir, dentry,
				      krg_tgid_base_stuff,
				      ARRAY_SIZE(krg_tgid_base_stuff));
}

static struct inode_operations krg_proc_tgid_base_inode_operations = {
	.lookup = krg_proc_tgid_base_lookup,
	.getattr = krg_pid_getattr,
	.setattr = proc_setattr,
};

static struct dentry *krg_proc_pid_instantiate(struct inode *dir,
					       struct dentry *dentry,
					       struct proc_distant_pid_info *task,
					       void *ptr)
{
	struct dentry *error = ERR_PTR(-ENOENT);
	struct inode *inode;

	inode =	krg_proc_pid_make_inode(dir->i_sb, task);
	if (!inode)
		goto out;

	inode->i_mode = S_IFDIR|S_IRUGO|S_IXUGO;
	inode->i_op = &krg_proc_tgid_base_inode_operations;
	inode->i_fop = &krg_proc_tgid_base_operations;
	inode->i_flags |= S_IMMUTABLE;
	inode->i_nlink = 4;
#ifdef CONFIG_SECURITY
	inode->i_nlink += 1;
#endif

	dentry->d_op = &krg_pid_dentry_operations;

	d_add(dentry, inode);
	error = NULL;

 out:
	return error;
}

static struct dentry *kcb_proc_pid_lookup(struct inode *dir,
					  struct dentry *dentry, pid_t tgid)
{
	//try and locate pid in the cluster
	struct proc_distant_pid_info task;
	struct task_kddm_object *obj;
	struct dentry *result = ERR_PTR(-ENOENT);

	task.pid = tgid;

	obj = kcb_task_readlock(tgid);
	if (!obj)
		goto out;
	task.task_obj = obj;
	task.dumpable = obj->dumpable;
	task.euid = obj->euid;
	task.egid = obj->egid;
	task.prob_node = obj->node;

#ifdef CONFIG_KRG_CAP
	if (task.prob_node == kerrighed_node_id
	    || !can_use_krg_cap(current, CAP_SEE_LOCAL_PROC_STAT))
#endif
	result = krg_proc_pid_instantiate(dir, dentry, &task, NULL);

 out:
	kcb_task_unlock(tgid);

	return result;
}

#define PROC_MAXPIDS 100

struct pid_list_msg {
	pid_t next_tgid;
};

static int krg_proc_pid_fill_cache(struct file *filp,
				   void *dirent, filldir_t filldir,
				   struct task_struct *task, int tgid)
{
	char name[PROC_NUMBUF];
	int len = snprintf(name, sizeof(name), "%d", tgid);
	struct proc_distant_pid_info proc_task;
	struct task_kddm_object *obj;
	int retval = 0;

	if (task)
		return proc_pid_fill_cache(filp, dirent, filldir, task, tgid);

	obj = kcb_task_readlock(tgid);
	if (obj) {
		kh_task_get(obj);
		proc_task.task_obj = obj;
		proc_task.pid = tgid;
		if (obj->node == KERRIGHED_NODE_ID_NONE)
			proc_task.prob_node = kerrighed_node_id;
		else
			proc_task.prob_node = obj->node;
		proc_task.dumpable = obj->dumpable;
		proc_task.euid = obj->euid;
		proc_task.egid = obj->egid;
	}
	kcb_task_unlock(tgid);

	if (obj) {
		retval = krg_proc_fill_cache(filp, dirent, filldir, name, len,
					     krg_proc_pid_instantiate,
					     &proc_task, NULL);
		kh_task_put(obj);
	}

	return retval;
}

/* Must be called under rcu_read_lock() */
static struct task_kddm_object *next_tgid(pid_t tgid)
{
	struct pid *pid;
	struct task_struct *task;
	struct task_kddm_object *task_obj;

retry:
	task_obj = NULL;
	pid = find_ge_pid(tgid);
	if (pid) {
		DEBUG(DEBUG_PROC_PID_FILES, 4, "pid=%d\n", pid->nr);
		tgid = pid->nr + 1;
		task = pid_task(pid, PIDTYPE_PID);
		if (task && !has_group_leader_pid(task))
			goto retry;
		if (task) {

			/* If task_obj is not NULL, it won't be freed until
			 * rcu_read_unlock() */
			task_obj = rcu_dereference(task->task_obj);
#ifdef CONFIG_KRG_EPM
			if (!task_obj)
				/* Try again in case task is migrating */
				task_obj = krg_pid_task(pid);
		} else {
			task_obj = krg_pid_task(pid);
		}
#endif
		DEBUG(DEBUG_PROC_PID_FILES, 4, "pid=%d task_obj=0x%p\n",
		      pid->nr, task_obj);
		if (!task_obj || task_obj->group_leader != task_obj->pid)
			goto retry;
	}

	return task_obj;
}

static void handle_req_available_tgids(struct rpc_desc *desc,
				       void *_msg, size_t size)
{
	struct pid_list_msg *msg = _msg;
	pid_t pid_array[PROC_MAXPIDS];
	pid_t tgid;
	struct task_kddm_object *task;
	int nr_tgids = 0;
	int retval;

	tgid = msg->next_tgid;
	BUG_ON(tgid < GLOBAL_PID_MASK);
	rcu_read_lock();
	for (task = next_tgid(tgid); task; task = next_tgid(tgid + 1)) {
		tgid = task->pid;
		DEBUG(DEBUG_PROC_PID_FILES, 4, "next tgid=%d\n", tgid);
		pid_array[nr_tgids++] = tgid;
		if (nr_tgids >= PROC_MAXPIDS)
			break;
	}
	rcu_read_unlock();

	retval = rpc_pack_type(desc, nr_tgids);
	if (retval)
		goto out_err_cancel;
	retval = rpc_pack_type(desc, pid_array);
	if (retval)
		goto out_err_cancel;

 out:
	return;

 out_err_cancel:
	rpc_cancel(desc);
	goto out;
}

static inline int fill_next_remote_tgids(kerrighed_node_t node,
					 struct file *filp,
					 void *dirent, filldir_t filldir,
					 loff_t offset)
{
	pid_t tgid;
	struct pid_list_msg msg;
	struct rpc_desc *desc;
	pid_t pid_array[PROC_MAXPIDS];
	int nr_pids;
#ifdef CONFIG_KRG_EPM
	struct pid *pid = NULL;
	struct task_struct *task = NULL;
#else
#define task NULL
#endif
	int i;
	int retval;

	tgid = filp->f_pos - offset;
	DEBUG(DEBUG_PROC_PID_FILES, 3, "node=%d, tgid=%d\n", node, tgid);
	if (tgid < GLOBAL_PID_MASK)
		tgid = GLOBAL_PID_MASK;
	msg.next_tgid = tgid;

	desc = rpc_begin(REQ_AVAILABLE_TGIDS, node);
	if (!desc)
		goto out_unlock;

	retval = rpc_pack_type(desc, msg);
	if (retval)
		goto err_cancel;

	retval = rpc_unpack_type(desc, nr_pids);
	if (retval)
		goto err_cancel;
	retval = rpc_unpack_type(desc, pid_array);
	if (retval)
		goto err_cancel;

	retval = rpc_end(desc, 0);
	if (retval)
		goto out_unlock;

	DEBUG(DEBUG_PROC_PID_FILES, 3, "%d pids\n", nr_pids);
	for (i = 0; i < nr_pids; i++) {
		tgid = pid_array[i];
		filp->f_pos = tgid + offset;
		DEBUG(DEBUG_PROC_PID_FILES, 4, "%d\n", tgid);
#ifdef CONFIG_KRG_EPM
		task = NULL;
		rcu_read_lock();
		pid = find_pid(tgid);
		if (pid) {
			task = pid_task(pid, PIDTYPE_PID);
			if (task)
				get_task_struct(task);
		}
		rcu_read_unlock();
#ifdef CONFIG_KRG_CAP
		if (!task
		    && can_use_krg_cap(current, CAP_SEE_LOCAL_PROC_STAT))
			continue;
#endif
#endif
		retval = krg_proc_pid_fill_cache(filp, dirent, filldir,
						 task, tgid);
#ifdef CONFIG_KRG_EPM
		if (task)
			put_task_struct(task);
#endif
		if (retval < 0) {
			DEBUG(DEBUG_PROC_PID_FILES, 2, "stop filling\n");
			retval = -EAGAIN;
			goto out;
		}
	}
	retval = nr_pids < ARRAY_SIZE(pid_array) ? 0 : nr_pids;

 out:
	DEBUG(DEBUG_PROC_PID_FILES, 3, "retval=%d\n", retval);
	return retval;

 out_unlock:
	retval = 0; /* Tell caller to proceed with next node */
	goto out;

 err_cancel:
	rpc_cancel(desc);
	rpc_end(desc, 0);
	goto out_unlock;
#ifndef CONFIG_KRG_EPM
#undef task
#endif
}

static inline int fill_next_local_tgids(struct file *filp,
					void *dirent, filldir_t filldir,
					loff_t offset)
{
	pid_t tgid = filp->f_pos - offset;
	pid_t next_tgid;
	struct pid *pid;
	struct task_struct *task;
	struct task_kddm_object *task_obj;
	int global_mode = tgid & GLOBAL_PID_MASK;
	int retval;

	rcu_read_lock();
	for (;;) {
		pid = find_ge_pid(tgid);
		if (!pid)
			break;
		if (!global_mode && (pid->nr & GLOBAL_PID_MASK))
			break;

		tgid = pid->nr + 1;
		task = pid_task(pid, PIDTYPE_PID);
		if (!task) {
#ifdef CONFIG_KRG_EPM
#ifdef CONFIG_KRG_CAP
			if (can_use_krg_cap(current, CAP_SEE_LOCAL_PROC_STAT))
				continue;
#endif
			/* Maybe a migrated thread group leader */
			task_obj = krg_pid_task(pid);
			if (!task_obj
			    || task_obj->pid != task_obj->group_leader)
#endif
				continue;
		} else if (has_group_leader_pid(task))
			get_task_struct(task);
		else
			continue;

		next_tgid = pid->nr;
		rcu_read_unlock();

		filp->f_pos = next_tgid + offset;
		retval = krg_proc_pid_fill_cache(filp, dirent, filldir,
						 task, next_tgid);
		if (task)
			put_task_struct(task);
		if (retval < 0)
			return retval; //EF: was -EAGAIN

		rcu_read_lock();
	}
	rcu_read_unlock();

	return 0;
}

static inline int __fill_next_tgids(kerrighed_node_t node,
				    struct file *filp,
				    void *dirent, filldir_t filldir,
				    loff_t offset)
{
	if (node == kerrighed_node_id)
		return fill_next_local_tgids(filp, dirent, filldir, offset);
	else
		return fill_next_remote_tgids(node,
					      filp, dirent, filldir, offset);
}

static int fill_next_tgids(kerrighed_node_t node,
			   struct file *filp,
			   void *dirent, filldir_t filldir,
			   loff_t offset)
{
	pid_t tgid;
	int retval;

	do {
		retval = __fill_next_tgids(node,
					   filp, dirent, filldir, offset);
		if (retval > 0) {
			tgid = filp->f_pos - offset;
			if ((tgid & INTERNAL_PID_MASK) >= PID_MAX_LIMIT - 1) {
				retval = 0;
				break;
			}
			/* Start from first tgid *not filled* for next chunk */
			filp->f_pos++;
		}
	} while (retval > 0);

	return retval;
}

static int kcb_proc_pid_readdir(struct file *filp,
				void *dirent, filldir_t filldir,
				loff_t offset)
{
	pid_t tgid;
	kerrighed_node_t node;
	int retval = 0;

	if ((unsigned long) filp->f_pos >=
	    (unsigned long)(KERRIGHED_PID_MAX_LIMIT + offset))
		goto out;

	/* First local PIDs */
	tgid = filp->f_pos - offset;
	if (!(tgid & GLOBAL_PID_MASK)) {
		retval = fill_next_tgids(kerrighed_node_id,
					 filp, dirent, filldir, offset);
		if (retval)
			goto out;
	}

	/* Second global PIDs */
	tgid = filp->f_pos - offset;
	if (!(tgid & GLOBAL_PID_MASK)) {
		tgid = MAKE_KERRIGHED_PID_FOR_NODE(0, 0);
		filp->f_pos = tgid + offset;
	}
	node = ORIG_NODE(tgid);
	for (; node < KERRIGHED_MAX_NODES;
	     node++,
	     filp->f_pos = MAKE_KERRIGHED_PID_FOR_NODE(0, node) + offset) {
		if (!krgnode_online(node))
			continue;
#if defined(CONFIG_KRG_CAP) && !defined(CONFIG_KRG_EPM)
		if (node != kerrighed_node_id
		    && can_use_krg_cap(current, CAP_SEE_LOCAL_PROC_STAT))
			continue;
#endif

		retval = fill_next_tgids(node, filp, dirent, filldir, offset);
		if (retval)
			break;
	}

 out:
	DEBUG(DEBUG_PROC_PID_FILES, 2, "retval=%d\n", retval);
	return retval;
}

/** Init cluster info stuffs.
 *  @author Renaud Lottiaux
 */
int proc_pid_info_init()
{
	DEBUG(DEBUG_PROC_PID_FILES, 1, "/proc/<pid> info management init\n");

	rpc_register_void(REQ_AVAILABLE_TGIDS, handle_req_available_tgids, 0);

	hook_register(&kh_proc_pid_readdir, kcb_proc_pid_readdir);
	hook_register(&kh_proc_pid_lookup, kcb_proc_pid_lookup);

	DEBUG(DEBUG_PROC_PID_FILES, 1,
	      "/proc/<pid> info management init : done\n");

	krg_proc_pid_init();

	return 0;
};

/** Init cluster info stuffs.
 *  @author Renaud Lottiaux
 */
int proc_pid_info_finalize()
{
	krg_proc_pid_finish();

	return 0;
};
