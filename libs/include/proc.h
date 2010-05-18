#ifndef LIBPROC_H
#define LIBPROC_H

#include <stdlib.h>
#include <unistd.h>

#include "types.h"
#include "checkpoint.h"

#define CHKPT_DIR "/var/chkpt"

/*
 * krg_check_checkpoint
 *
 * Check if kerrighed support checkpoint
 *
 * Return 0 if ok, -1 otherwise
 */
int krg_check_checkpoint(void);

int get_node_id(void);
int get_nr_nodes(void);

/* process migration system calls */
int migrate (pid_t, int);
int migrate_self (int destination_node);
int thread_migrate (pid_t thread_id, int destination_node);

/* checkpoint/restart/rollback system calls */
int application_freeze_from_appid(long app_id);
int application_freeze_from_pid(pid_t pid);

int application_unfreeze_from_appid(long app_id, int signal);
int application_unfreeze_from_pid(pid_t pid, int signal);

struct checkpoint_info application_checkpoint_from_appid(long app_id,
							 int flags);
struct checkpoint_info application_checkpoint_from_pid(pid_t pid,
						       int flags);

/* return the pid of the application root process in case of success */
int application_restart(long app_id, int chkpt_sn, int flags,
			struct cr_subst_files_array *substitution);

int application_set_userdata(__u64 data);
int application_get_userdata_from_appid(long app_id, __u64 *data);
int application_get_userdata_from_pid(pid_t pid, __u64 *data);

#endif // LIBPROC_H
