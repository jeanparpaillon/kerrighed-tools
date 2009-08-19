#ifndef LIBIPC_H
#define LIBIPC_H

int ipc_msgq_checkpoint(int msqid, int fd);

int ipc_msgq_restart(int fd);

#endif
