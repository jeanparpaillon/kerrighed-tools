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

#include <kerrighed/types.h>
#include <kerrighed/krgnodemask.h>
#include <epm/migration_types.h>
/* #include <proc/krg_fork_types.h> */

#include <checkpoint.h>
#include <kerrighed_tools.h>


#define KRG_SERVICES_PATH "/proc/kerrighed/services"

//#define DEBUG_LIBARAGORN

extern pid_t __libc_fork();



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

int get_nr_cpu (void)
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
	checkpoint_infos_t ckpt_infos;

	ckpt_infos.app_id = app_id;
	ckpt_infos.type = FROM_APPID;
	ckpt_infos.signal = 0;
	r = call_kerrighed_services(KSYS_APP_FREEZE, &ckpt_infos);

	return r;
}

int application_freeze_from_pid(pid_t pid)
{
	int r;
	checkpoint_infos_t ckpt_infos;

	ckpt_infos.app_id = pid;
	ckpt_infos.type = FROM_PID;
	ckpt_infos.signal = 0;
	r = call_kerrighed_services(KSYS_APP_FREEZE, &ckpt_infos);

	return r;
}

int application_unfreeze_from_appid(long app_id, int signal)
{
	int r;
	checkpoint_infos_t ckpt_infos;

	ckpt_infos.app_id = app_id;
	ckpt_infos.type = FROM_APPID;
	ckpt_infos.signal = signal;
	r = call_kerrighed_services(KSYS_APP_UNFREEZE, &ckpt_infos);

	return r;
}

int application_unfreeze_from_pid(pid_t pid, int signal)
{
	int r;
	checkpoint_infos_t ckpt_infos;

	ckpt_infos.app_id = pid;
	ckpt_infos.type = FROM_PID;
	ckpt_infos.signal = signal;
	r = call_kerrighed_services(KSYS_APP_UNFREEZE, &ckpt_infos);

	return r;
}

checkpoint_infos_t application_checkpoint_from_appid(long app_id)
{
	checkpoint_infos_t ckpt_infos;

	ckpt_infos.app_id = app_id;
	ckpt_infos.chkpt_sn = 0;
	ckpt_infos.type = FROM_APPID;
	ckpt_infos.signal = 0;
	ckpt_infos.result = call_kerrighed_services(KSYS_APP_CHKPT,
						    &ckpt_infos);

	return ckpt_infos;
}

checkpoint_infos_t application_checkpoint_from_pid(pid_t pid)
{
	checkpoint_infos_t ckpt_infos;

	ckpt_infos.app_id = pid;
	ckpt_infos.chkpt_sn = 0;
	ckpt_infos.type = FROM_PID;
	ckpt_infos.signal = 0;
	ckpt_infos.result = call_kerrighed_services(KSYS_APP_CHKPT,
						    &ckpt_infos);

	return ckpt_infos;
}

int application_restart(long app_id, int chkpt_sn, int flags)
{
	int res;
	restart_request_t rst_req;

	rst_req.app_id = app_id;
	rst_req.chkpt_sn = chkpt_sn;
	rst_req.flags = flags;

	res = call_kerrighed_services(KSYS_APP_RESTART, &rst_req);

	/* in case of success, rst_req.app_id has been replaced by the
	 * application root process id.
	 */
	if (!res)
		res = rst_req.app_id;

	return res;
}

int application_set_userdata(__u64 data)
{
	int r = call_kerrighed_services(KSYS_APP_SET_USERDATA, &data);
	return r;
}

int application_get_userdata_from_appid(long app_id, __u64 *data)
{
	int res;
	app_userdata_request_t datareq;
	datareq.app_id = app_id;
	datareq.type = FROM_APPID;
	datareq.user_data = 0;

	res = call_kerrighed_services(KSYS_APP_GET_USERDATA, &datareq);
	if (!res)
		*data = datareq.user_data;
	return res;
}

int application_get_userdata_from_pid(long app_id, __u64 *data)
{
	int res;
	app_userdata_request_t datareq;
	datareq.app_id = app_id;
	datareq.type = FROM_PID;
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
