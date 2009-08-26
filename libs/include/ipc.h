#ifndef LIBIPC_H
#define LIBIPC_H

int ipc_msgq_checkpoint(int msqid, int fd);

int ipc_msgq_restart(int fd);

int ipc_sem_checkpoint(int semid, int fd);

int ipc_sem_restart(int fd);

int ipc_shm_checkpoint(int shmid, int fd);

int ipc_shm_restart(int fd);

#endif
