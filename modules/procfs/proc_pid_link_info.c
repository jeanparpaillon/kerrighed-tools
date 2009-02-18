/*
 *  Copyright (C) 2001-2006, INRIA, Universite de Rennes 1, EDF.
 *  Copyright (C) 2007, Louis Rilling - Kerlabs.
 */

#include <linux/proc_fs.h>
#include <linux/mount.h>
#include <linux/namei.h>
#include <asm/uaccess.h>
#include <kerrighed/procfs.h>

#define MODULE_NAME "proc pid link"
#include "debug_procfs.h"

#ifndef FILE_NONE
#  if defined(FILE_PROC_INFO_FILE) || defined(FILE_ALL)
#     define DEBUG_THIS_MODULE
#  endif
#endif

#include <tools/debug.h>
#include <proc/task.h>

#include "proc_pid_link_info.h"

static int krg_proc_fd_access_allowed(struct inode *inode)
{
	struct proc_distant_pid_info *task = get_krg_proc_task(inode);
/* 	struct task_kddm_object *obj; */
	int allowed = 0;

/* 	obj = kcb_task_readlock(task->pid); */
/* 	if (obj) { */
		if (((current->uid != task->euid) ||
/*		     (current->uid != obj->suid) || */
/* 		     (current->uid != obj->uid) || */
		     (current->gid != task->egid)/*  || */
/*		     (current->gid != obj->sgid) || */
/*		     (current->gid != obj->gid) */) && !capable(CAP_SYS_PTRACE))
			allowed = -EPERM;
		if (!task->dumpable && !capable(CAP_SYS_PTRACE))
			allowed = -EPERM;
/* 	} */
/* 	kcb_task_unlock(task->pid); */
	return allowed;
}

static void *krg_proc_pid_follow_link(struct dentry *dentry,
				      struct nameidata *nd)
{
	struct inode *inode = dentry->d_inode;
	int error = -EACCES;

	/* We don't need a base pointer in the /proc filesystem */
	path_release(nd);

	/* Are we allowed to snoop on the tasks file descriptors? */
	if (!krg_proc_fd_access_allowed(inode))
		goto out;

	error = get_krg_proc_task(inode)->op.proc_get_link(inode, &nd->dentry,
							   &nd->mnt);
	nd->last_type = LAST_BIND;
 out:
	return ERR_PTR(error);
}

static int krg_proc_pid_readlink(struct dentry *dentry,
				 char __user *buffer, int buflen)
{
	int error = -EACCES;
	struct inode *inode = dentry->d_inode;
	struct dentry *de;
	struct vfsmount *mnt = NULL;

	/* Are we allowed to snoop on the tasks file descriptors? */
	if (!krg_proc_fd_access_allowed(inode))
		goto out;

	error = get_krg_proc_task(inode)->op.proc_get_link(inode, &de, &mnt);
	if (error)
		goto out;

	error = do_proc_readlink(de, mnt, buffer, buflen);
	dput(de);
	mntput(mnt);
 out:
	return error;
}

struct inode_operations krg_proc_pid_link_inode_operations = {
	.readlink = krg_proc_pid_readlink,
	.follow_link = krg_proc_pid_follow_link,
	.setattr = proc_setattr,
};

int krg_proc_exe_link(struct inode *inode, struct dentry **dentry,
		      struct vfsmount **mnt)
{
	return 0;
}

int krg_proc_cwd_link(struct inode *inode, struct dentry **dentry,
		      struct vfsmount **mnt)
{
	return 0;
}

int krg_proc_root_link(struct inode *inode, struct dentry **dentry,
		       struct vfsmount **mnt)
{
	//should increment fs of task at distance
	return 0;
}
