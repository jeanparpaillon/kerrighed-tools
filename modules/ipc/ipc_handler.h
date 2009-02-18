/** Interface of IPC management.
 *  @file ipc_handler.h
 *
 *  @author Renaud Lottiaux, Matthieu Fertré
 */


#ifndef IPC_HANDLER_H
#define IPC_HANDLER_H

#include <linux/ipc.h>

int free_ipc_id(struct ipc_ids* ids, int index);

void ipc_handler_finalize (void);
void ipc_handler_init (void);

#endif // IPC_HANDLER_H
