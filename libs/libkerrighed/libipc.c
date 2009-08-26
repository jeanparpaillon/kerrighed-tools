/*
 * IPC related interface functions
 *
 */

#include <stdio.h>
#include <errno.h>

#include <kerrighed_tools.h>
#include <ipc.h>

int ipc_msgq_checkpoint(int msqid, int fd)
{
	int args[2];
	args[0] = msqid;
	args[1] = fd;

	return call_kerrighed_services(KSYS_IPC_MSGQ_CHKPT, args);
}

int ipc_msgq_restart(int fd)
{
	return call_kerrighed_services(KSYS_IPC_MSGQ_RESTART, &fd);
}

int ipc_sem_checkpoint(int semid, int fd)
{
	int args[2];
	args[0] = semid;
	args[1] = fd;

	return call_kerrighed_services(KSYS_IPC_SEM_CHKPT, args);
}

int ipc_sem_restart(int fd)
{
	return call_kerrighed_services(KSYS_IPC_SEM_RESTART, &fd);
}
