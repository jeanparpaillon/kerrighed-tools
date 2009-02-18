/**  /proc/<pid>/fd information management.
 *  @file proc_fd_info.h
 *
 *  @author David Margery
 */

#ifndef __PROC_PID_MISC_H__
#define __PROC_PID_MISC_H__

#include <linux/fs.h>

extern struct file_operations krg_proc_maps_operations;

extern struct inode_operations krg_proc_mem_inode_operations;
extern struct file_operations krg_proc_mem_operations;

extern struct file_operations krg_proc_mounts_operations;

#endif /* __PROC_PID_MISC_H__*/
