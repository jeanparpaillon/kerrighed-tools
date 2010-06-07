/** Process related interface functions.
 *  @file libaragorn.c
 *
 *  Copyright (C) 2001-2006, INRIA, Universite de Rennes 1, EDF.
 */

#include <stdio.h>
#include <sys/syscall.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>

#include <types.h>
#include <krgnodemask.h>
#include <migration.h>
#include <checkpoint.h>
#include <kerrighed_tools.h>
#include <proc.h>

/*****************************************************************************/
/*                                                                           */
/*                              EXPORTED FUNCTIONS                           */
/*                                                                           */
/*****************************************************************************/

/* Return the node id of the local machine */

int get_node_id (void)
{
  int res;
  int node_id;

  res = call_kerrighed_services(KSYS_GET_NODE_ID, &node_id);

  if (res != 0)
    return -1;

  return node_id;
}


/* Return the number of nodes in the cluster */

int get_nr_nodes(void)
{
  int res;
  int count;

  res = call_kerrighed_services(KSYS_GET_NODES_COUNT, &count);

  if (res != 0)
    return -1;

  return count;
}


/** Migrate a process.
 *  @author Geoffroy Vallée
 *
 *  @param pid          Pid of process to migrate.
 *  @param destination  Node ID of the destination node.
 */
int migrate (pid_t pid, int destination_node)
{
  int res;
  migration_infos_t migration_infos;

  migration_infos.process_to_migrate = pid;
  migration_infos.destination_node_id = destination_node;

  res = call_kerrighed_services(KSYS_PROCESS_MIGRATION,  &migration_infos);

  return res;
}


int migrate_self (int destination_node)
{
  int res;
  migration_infos_t migration_infos;

  migration_infos.process_to_migrate = getpid();
  migration_infos.destination_node_id = destination_node;

  res = call_kerrighed_services(KSYS_PROCESS_MIGRATION, &migration_infos);

  return res ;
}

int application_freeze_from_appid(long app_id)
{
	int r;
	struct checkpoint_info ckpt_info;

	ckpt_info.app_id = app_id;
	ckpt_info.flags = 0;
	ckpt_info.signal = 0;
	r = call_kerrighed_services(KSYS_APP_FREEZE, &ckpt_info);

	return r;
}

int application_freeze_from_pid(pid_t pid)
{
	int r;
	struct checkpoint_info ckpt_info;

	ckpt_info.app_id = pid;
	ckpt_info.flags = APP_FROM_PID;
	ckpt_info.signal = 0;
	r = call_kerrighed_services(KSYS_APP_FREEZE, &ckpt_info);

	return r;
}

int application_unfreeze_from_appid(long app_id, int signal)
{
	int r;
	struct checkpoint_info ckpt_info;

	ckpt_info.app_id = app_id;
	ckpt_info.flags = 0;
	ckpt_info.signal = signal;
	r = call_kerrighed_services(KSYS_APP_UNFREEZE, &ckpt_info);

	return r;
}

int application_unfreeze_from_pid(pid_t pid, int signal)
{
	int r;
	struct checkpoint_info ckpt_info;

	ckpt_info.app_id = pid;
	ckpt_info.flags = APP_FROM_PID;
	ckpt_info.signal = signal;
	r = call_kerrighed_services(KSYS_APP_UNFREEZE, &ckpt_info);

	return r;
}

struct checkpoint_info application_checkpoint_from_appid(long app_id, int flags)
{
	struct checkpoint_info ckpt_info;

	ckpt_info.app_id = app_id;
	ckpt_info.chkpt_sn = 0;
	ckpt_info.flags = flags;
	ckpt_info.signal = 0;
	ckpt_info.result = call_kerrighed_services(KSYS_APP_CHKPT,
						    &ckpt_info);

	return ckpt_info;
}

struct checkpoint_info application_checkpoint_from_pid(pid_t pid, int flags)
{
	struct checkpoint_info ckpt_info;

	ckpt_info.app_id = pid;
	ckpt_info.chkpt_sn = 0;
	ckpt_info.flags = flags | APP_FROM_PID;
	ckpt_info.signal = 0;
	ckpt_info.result = call_kerrighed_services(KSYS_APP_CHKPT,
						    &ckpt_info);

	return ckpt_info;
}

int application_restart(long app_id, int chkpt_sn, int flags,
			struct cr_subst_files_array *substitution)
{
	int res, i;
	struct restart_request rst_req;
	size_t subst_str_len;

	rst_req.app_id = app_id;
	rst_req.chkpt_sn = chkpt_sn;
	rst_req.flags = flags;

	rst_req.substitution = *substitution;

	if (rst_req.substitution.nr) {

		subst_str_len = sizeof(kerrighed_node_t)*2 +
			sizeof(unsigned long)*2;

		for (i = 0; i < rst_req.substitution.nr; i++) {

			if (strlen(substitution->files[i].file_id)
			    != subst_str_len)
				goto err_inval;

			if (fcntl(substitution->files[i].fd, F_GETFL) == -1)
				goto err_inval;
		}

	} else if (rst_req.substitution.files)
		goto err_inval;

	res = call_kerrighed_services(KSYS_APP_RESTART, &rst_req);
	if (!res)
		res = rst_req.root_pid;

error:
	return res;
err_inval:
	res = -1;
	errno = EINVAL;
	goto error;
}

int application_set_userdata(__u64 data)
{
	int r = call_kerrighed_services(KSYS_APP_SET_USERDATA, &data);
	return r;
}

int application_get_userdata_from_appid(long app_id, __u64 *data)
{
	int res;
	struct app_userdata_request datareq;
	datareq.app_id = app_id;
	datareq.flags = 0;
	datareq.user_data = 0;

	res = call_kerrighed_services(KSYS_APP_GET_USERDATA, &datareq);
	if (!res)
		*data = datareq.user_data;
	return res;
}

int application_get_userdata_from_pid(pid_t pid, __u64 *data)
{
	int res;
	struct app_userdata_request datareq;
	datareq.app_id = pid;
	datareq.flags = APP_FROM_PID;
	datareq.user_data = 0;

	res = call_kerrighed_services(KSYS_APP_GET_USERDATA, &datareq);
	if (!res)
		*data = datareq.user_data;

	return res;
}

int thread_migrate (pid_t thread_id, int destination_node)
{
  int res;
  migration_infos_t migration_infos;

  migration_infos.thread_to_migrate = thread_id ;
  migration_infos.destination_node_id = destination_node;

  res = call_kerrighed_services(KSYS_THREAD_MIGRATION, &migration_infos);

  return res;
}
