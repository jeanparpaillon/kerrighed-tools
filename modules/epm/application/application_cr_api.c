/*
 *  Kerrighed/modules/epm/application_cr_api.c
 *
 *  Copyright (C) 1999-2006 INRIA, Universite de Rennes 1, EDF
 *  Copyright (C) 2007-2008 Matthieu Fertré - INRIA
 */

#define MODULE_NAME "Application C/R API"

#include <epm/debug_epm.h>

#include <epm/application/application.h>
#include <epm/application/application_cr_api.h>
#include <epm/application/app_checkpoint.h>
#include <epm/application/app_restart.h>

/*****************************************************************************/
/*                                                                           */
/*                                SYS CALL FUNCTIONS                         */
/*                                                                           */
/*****************************************************************************/

/** System call function to freeze an application.
 *  @author Matthieu Fertré
 *
 */
int sys_app_freeze(checkpoint_infos_t *infos)
{
	int r;
	credentials_t creds;

	DEBUG(DBG_CKPT_API, 1, "Freezing application %ld\n", infos->app_id);

	creds.uid = current->uid;
	creds.euid = current->euid;
	creds.gid = current->gid;

	r = app_freeze(infos, &creds);

	return r;
}

/** System call function to unfreeze an application.
 *  @author Matthieu Fertré
 *
 */
int sys_app_unfreeze(checkpoint_infos_t *infos)
{
	int r;
	credentials_t creds;

	DEBUG(DBG_CKPT_API, 1, "Unfreezing application %ld\n", infos->app_id);

	creds.uid = current->uid;
	creds.euid = current->euid;
	creds.gid = current->gid;

	r = app_unfreeze(infos, &creds);

	return r;
}

/** System call function to checkpoint an application.
 *  @author Matthieu Fertré
 *
 */
int sys_app_chkpt(checkpoint_infos_t *infos)
{
	int r;
	credentials_t creds;

	DEBUG(DBG_CKPT_API, 1, "Checkpoint application %ld\n", infos->app_id);

	creds.uid = current->uid;
	creds.euid = current->euid;
	creds.gid = current->gid;

	r = app_chkpt(infos, &creds);

	DEBUG(DBG_CKPT_API, 1,
	      "Checkpoint application %ld : done with err %d\n", infos->app_id,
	      r);

	return r;
}


/** System call function to checkpoint an application
 *  @author Matthieu Fertré
 *
 */
int sys_app_restart(restart_request_t *req)
{
	int r;
	credentials_t creds;
	task_identity_t requester;

	DEBUG(DBG_CKPT_API, 1, "Restart application with %ld-%d \n",
	      req->app_id, req->chkpt_sn);

	creds.uid = current->uid;
	creds.euid = current->euid;
	creds.gid = current->gid;

	requester.pid = current->pid;
	requester.tgid = current->tgid;

	r = app_restart(req, &creds, &requester);

	DEBUG(DBG_CKPT_API, 1,
	      "Restart application %ld-%d : done with err %d\n", req->app_id,
	      req->chkpt_sn, r);

	return r;
}
