/** Global /proc/<pid> management
 *  @file proc_pid_misc.c
 *  
 *  Copyright (C) 2001-2006, INRIA, Universite de Rennes 1, EDF.
 */

#include <kerrighed/procfs.h>

#define MODULE_NAME "proc pid misc info"
#include "debug_procfs.h"

#ifndef FILE_NONE
#  if defined(FILE_PROC_PID_MISC_FILE) || defined(FILE_ALL)
#     define DEBUG_THIS_MODULE
#  endif
#endif

#include <tools/debug.h>

#include <linux/proc_fs.h>
#include <linux/seq_file.h>

static int krg_maps_open(struct inode *inode, struct file *file)
{
	struct proc_distant_pid_info *task = get_krg_proc_task(inode);
	//int ret = seq_open(file, &kerrighed_proc_pid_maps_op);
	int ret = seq_open(file, NULL);
	if (!ret) {
		struct seq_file *m = file->private_data;
		m->private = task;
	}
	return ret;
}

struct file_operations krg_proc_maps_operations = {
	.open = krg_maps_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

/**
struct inode_operations kerrighed_proc_mem_inode_operations ;
struct file_operations kerrighed_proc_mem_operations ;
*/
/*
struct file_operations kerrighed_proc_mounts_operations ;
*/
