/**  /proc/<pid>/ information management.
 *  @file proc_fd_info.h
 *
 *  @author David Margery
 */

#ifndef __PROC_PID_FILE_INFO_H__
#define __PROC_PID_FILE_INFO_H__

#include <linux/fs.h>

struct proc_distant_pid_info;

extern struct file_operations krg_proc_info_file_operations;

int krg_proc_pid_environ(struct proc_distant_pid_info *task, char *buffer);
int krg_proc_pid_status(struct proc_distant_pid_info *task, char *buffer);
int krg_proc_tgid_stat(struct proc_distant_pid_info *task, char *buffer);
int krg_proc_pid_cmdline(struct proc_distant_pid_info *task, char *buffer);
int krg_proc_pid_statm(struct proc_distant_pid_info *task, char *buffer);

void krg_proc_pid_init(void);
void krg_proc_pid_finish(void);

#endif /* __PROC_PID_FILE_INFO_H__ */
