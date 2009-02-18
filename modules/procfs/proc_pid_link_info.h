/**  /proc/<pid>/fd information management.
 *  @file proc_fd_info.h
 *
 *  @author David Margery
 */

#ifndef __PROC_PID_LINK_INFO_H__
#define __PROC_PID_LINK_INFO_H__

#include <linux/fs.h>

extern struct inode_operations krg_proc_pid_link_inode_operations;

int krg_proc_exe_link(struct inode *inode, struct dentry **dentry,
		      struct vfsmount **mnt);
int krg_proc_cwd_link(struct inode *inode, struct dentry **dentry,
		      struct vfsmount **mnt);
int krg_proc_root_link(struct inode *inode, struct dentry **dentry,
		       struct vfsmount **mnt);

#endif /* __PROC_PID_LINK_INFO_H__ */
