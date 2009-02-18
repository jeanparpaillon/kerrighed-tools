/** Access to Physical File System management.
 *  @file physical_fs.c
 *
 *  Copyright (C) 2001-2006, INRIA, Universite de Rennes 1, EDF.
 *  Copyright (C) 2006-2007, Renaud Lottiaux, Kerlabs.
 *  
 *  @author Renaud Lottiaux 
 */

#include "debug_fs.h"

#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/module.h>
#ifdef CONFIG_X86_64
#include <asm/ia32.h>
#endif
#include <linux/file.h>
#include <linux/namei.h>

#include "physical_fs.h"


char *physical_d_path (struct dentry *dentry,
                       struct vfsmount *mnt,
                       char *tmp)
{
	char *path;
	int len;
	
	path = __d_path (dentry, mnt, init_task.fs->root, init_task.fs->rootmnt,
			 tmp, PAGE_SIZE);
	
	if (IS_ERR (path))
		return NULL;
	
	len = strlen (path);
	if (len >= 10) {
		if (strcmp (path + len - 10, " (deleted)") == 0)
			path[len - 10] = 0;
	}
	
	return path;
}



struct file *open_physical_file (char *filename,
                                 int flags,
                                 int mode,
                                 uid_t uid,
                                 gid_t gid)
{
	struct dentry *saved_root;
	struct vfsmount *saved_mnt;
	uid_t saved_uid;
	gid_t saved_gid;
	struct file *file;
	
	saved_mnt = current->fs->rootmnt;
	saved_root = current->fs->root;
	saved_uid = current->fsuid;
	saved_gid = current->fsgid;
	
	current->fs->rootmnt = init_task.fs->rootmnt;
	current->fs->root = init_task.fs->root;
	current->fsuid = uid;
	current->fsgid = gid;
	
	file = filp_open (filename, flags, mode);
	
	current->fsuid = saved_uid;
	current->fsgid = saved_gid;
	current->fs->rootmnt = saved_mnt;
	current->fs->root = saved_root;
	
	return file;
}



int close_physical_file (struct file *file)
{
	int res;
	
	res = filp_close (file, current->files);
	
	return res;
}



int remove_physical_file (struct file *file)
{
	struct dentry *dentry;
	struct inode *dir;
	int res = 0;
	
	dentry = file->f_dentry;
	dir = dentry->d_parent->d_inode;
	
	res = vfs_unlink (dir, dentry);
	dput (dentry);
	put_filp (file);
	
	return res;
}

int remove_physical_dir (struct file *file)
{
	struct dentry *dentry;
	struct inode *dir;
	int res = 0;
	
	dentry = file->f_dentry;
	dir = dentry->d_parent->d_inode;
	
	res = vfs_rmdir (dir, dentry);
	dput (dentry);
	put_filp (file);
	
	return res;
}
