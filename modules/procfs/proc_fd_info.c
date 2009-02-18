/** Global /proc/<pid> management
 *  @file proc_fd_info.c
 *
 *  Copyright (C) 2001-2006, INRIA, Universite de Rennes 1, EDF.
 *  Copyright (C) 2007, Louis Rilling - Kerlabs.
 */

#include <linux/kernel.h>
#include <linux/fs.h>
#include <kerrighed/procfs.h>

#define MODULE_NAME "proc fd info"
#include "debug_procfs.h"

#ifndef FILE_NONE
#  if defined(FILE_PROC_FD_INFO_FILE) || defined(FILE_ALL)
#     define DEBUG_THIS_MODULE
#  endif
#endif

static int krg_proc_readfd(struct file *filp, void *dirent, filldir_t filldir)
{
	printk("krg_proc_readfd\n");
	return 0;
}

struct file_operations krg_proc_fd_operations = {
	.read = generic_read_dir,
	.readdir = krg_proc_readfd,
};

static struct dentry *krg_proc_lookupfd(struct inode *dir,
					struct dentry *dentry,
					struct nameidata *nd)
{
	printk("krg_proc_lookupfd\n");
	return ERR_PTR(-ENOENT);
}

/*
 * proc directories can do almost nothing..
 */
struct inode_operations krg_proc_fd_inode_operations = {
	.lookup = krg_proc_lookupfd,
	.setattr = proc_setattr,
};
