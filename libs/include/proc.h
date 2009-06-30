#ifndef LIBPROC_H
#define LIBPROC_H

#include <stdlib.h>
#include <unistd.h>

#include "types.h"
#include "checkpoint.h"

int get_node_id(void);
int get_nr_cpu(void);

/* process migration system calls */
int migrate (pid_t, int);
int migrate_self (int destination_node);
int thread_migrate (pid_t thread_id, int destination_node);

/* checkpoint/restart/rollback system calls */
int application_freeze_from_appid(long app_id);
int application_freeze_from_pid(long app_id);

int application_unfreeze_from_appid(long app_id, int signal);
int application_unfreeze_from_pid(long app_id, int signal);

checkpoint_infos_t application_checkpoint_from_appid (media_t media, long app_id);
checkpoint_infos_t application_checkpoint_from_pid(media_t media, pid_t pid);

/* return the pid of the application root process in case of success */
int application_restart(media_t media, long app_id, int chkpt_sn, int flags);

#endif // LIBPROC_H
