/**  /proc/<pid>/fd information management.
 *  @file proc_fd_info.h
 *
 *  @author David Margery
 */

#ifndef __PROC_FD_INFO_H__
#define __PROC_FD_INFO_H__

#include <linux/fs.h>

extern struct file_operations krg_proc_fd_operations;
extern struct inode_operations krg_proc_fd_inode_operations;

#endif /* __PROC_FD_INFO_H__ */
