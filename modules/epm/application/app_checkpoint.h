/** Application checkpoint
 *  @author Matthieu Fertré
 */

#ifndef __APPLICATION_CHECKPOINT_H__
#define __APPLICATION_CHECKPOINT_H__

#include <kerrighed/sys/checkpoint.h>
#include <epm/application/application.h>

int app_freeze(checkpoint_infos_t *infos,
	       credentials_t *user_creds);

int app_unfreeze(checkpoint_infos_t *infos,
		 credentials_t *user_creds);

int app_chkpt(checkpoint_infos_t *infos,
	      const credentials_t *user_creds);

void application_checkpoint_rpc_init(void);

#endif
