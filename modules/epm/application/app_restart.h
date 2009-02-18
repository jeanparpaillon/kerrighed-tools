/** Application restart
 *  @author Matthieu Fertré
 */

#ifndef __APPLICATION_RESTART_H__
#define __APPLICATION_RESTART_H__

int app_restart(restart_request_t *req, const credentials_t *user_creds,
		const task_identity_t *requester);

void application_restart_rpc_init(void);

#endif
