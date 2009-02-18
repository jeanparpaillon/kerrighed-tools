/*
 *  Kerrighed/modules/epm/procfs.c
 *
 *  Copyright (C) 1999-2006 INRIA, Universite de Rennes 1, EDF
 */

/** /proc manager
 *  @file aragorn_proc.c
 *
 *  @author Geoffroy Vallée.
 */

#include <linux/proc_fs.h>
#include <asm/uaccess.h>

#include "debug_epm.h"

#define MODULE_NAME "Aragorn Proc "

#include <tools/krg_syscalls.h>
#include <tools/krg_services.h>
#include <tools/procfs.h>
#include <epm/migration_types.h>
#include <epm/migration_api.h>
#include <epm/application/application_cr_api.h>


static struct proc_dir_entry *proc_epm = NULL;


/** /proc function call to migrate a task
 *  @author Geoffroy Vallee
 *
 *  @param arg     Migration arguments from user space.
 */
int proc_migrate_process(void *arg)
{
	migration_infos_t migration_info;

	DEBUG(DBG_PROCFS, 1, "Starting...\n");

	if (copy_from_user((void *) &migration_info, (void *) arg,
			   sizeof(migration_infos_t)))
		return -EFAULT;

	return sys_migrate_process(migration_info.process_to_migrate,
				   migration_info.destination_node_id);
}


/** /proc function call to migrate a thread
 *  @author Geoffroy Vallee
 *
 *  @param arg     Migration arguments from user space.
 */
int proc_migrate_thread(void *arg)
{
	migration_infos_t migration_info;

	DEBUG(DBG_PROCFS, 1, "Starting...\n");

	if (copy_from_user((void *) &migration_info, (void *) arg,
			   sizeof(migration_infos_t)))
		return -EFAULT;

	return sys_migrate_thread(migration_info.thread_to_migrate,
				  migration_info.destination_node_id);
}

/** /proc function call to freeze an application.
 *  @author Matthieu Fertré
 */
int proc_app_freeze(void *arg)
{
	checkpoint_infos_t ckpt_infos;

	if (copy_from_user((void *) &ckpt_infos, (void *) arg,
			   sizeof(checkpoint_infos_t)))
		return -EFAULT;

	return sys_app_freeze(&ckpt_infos);
}
/** /proc function call to unfreeze an application.
 *  @author Matthieu Fertré
 */
int proc_app_unfreeze(void *arg)
{
	checkpoint_infos_t ckpt_infos;

	if (copy_from_user((void *) &ckpt_infos, (void *) arg,
			   sizeof(checkpoint_infos_t)))
		return -EFAULT;

	return sys_app_unfreeze(&ckpt_infos);
}

/** /proc function call to checkpoint an application.
 *  @author Matthieu Fertre
 *
 *  @param pid      Pid of one of the application processes
 */
int proc_app_chkpt(void *arg)
{
	int res;
	checkpoint_infos_t ckpt_infos;

	if (copy_from_user((void *) &ckpt_infos, (void *) arg,
			   sizeof(checkpoint_infos_t)))
		return -EFAULT;

	res = sys_app_chkpt(&ckpt_infos);

	if (copy_to_user(arg, &ckpt_infos, sizeof(ckpt_infos)))
		return -EFAULT;

	return res;
}


/** /proc function call to restart a checkpointed application.
 *  @author Matthieu Fertre
 *
 *  @param pid      Pid of one of the application processes
 *  @param version  Version of checkpoint
 */
int proc_app_restart(void *arg)
{
	restart_request_t restart_req;

	if (copy_from_user
	    ((void *) &restart_req, (void *) arg, sizeof(restart_request_t)))
		return -EFAULT;

	return sys_app_restart(&restart_req);
}


int epm_procfs_start(void)
{
	int r;
	int err = -EINVAL;

	/* /proc/kerrighed/aragorn */

	proc_epm = create_proc_entry("epm", S_IFDIR | 0755, proc_kerrighed);

	if (proc_epm == NULL)
		return -ENOMEM;

	r = register_proc_service(KSYS_PROCESS_MIGRATION, proc_migrate_process);
	if (r != 0)
		goto err;

	r = register_proc_service(KSYS_THREAD_MIGRATION, proc_migrate_thread);
	if (r != 0)
		goto unreg_migrate_process;

	r = register_proc_service(KSYS_APP_FREEZE, proc_app_freeze);
	if (r != 0)
		goto unreg_migrate_thread;

	r = register_proc_service(KSYS_APP_UNFREEZE, proc_app_unfreeze);
	if (r != 0)
		goto unreg_app_freeze;

	r = register_proc_service(KSYS_APP_CHKPT, proc_app_chkpt);
	if (r != 0)
		goto unreg_app_unfreeze;

	r = register_proc_service(KSYS_APP_RESTART, proc_app_restart);
	if (r != 0)
		goto unreg_app_chkpt;
	return 0;

	unregister_proc_service(KSYS_APP_RESTART);
 unreg_app_chkpt:
	unregister_proc_service(KSYS_APP_CHKPT);
 unreg_app_unfreeze:
	unregister_proc_service(KSYS_APP_UNFREEZE);
 unreg_app_freeze:
	unregister_proc_service(KSYS_APP_FREEZE);
 unreg_migrate_thread:
	unregister_proc_service(KSYS_THREAD_MIGRATION);
 unreg_migrate_process:
	unregister_proc_service(KSYS_PROCESS_MIGRATION);
 err:
	return err;
}


void epm_procfs_exit(void)
{
	DEBUG(DBG_PROCFS, 1, "starting...\n");

	unregister_proc_service(KSYS_PROCESS_MIGRATION);
	unregister_proc_service(KSYS_THREAD_MIGRATION);
	unregister_proc_service(KSYS_APP_FREEZE);
	unregister_proc_service(KSYS_APP_UNFREEZE);
	unregister_proc_service(KSYS_APP_CHKPT);
	unregister_proc_service(KSYS_APP_RESTART);

	DEBUG(DBG_PROCFS, 2, "procfs_deltree proc_aragorn\n");
	procfs_deltree(proc_epm);

	DEBUG(DBG_PROCFS, 1, "aragorn services unregistered\n");
}
