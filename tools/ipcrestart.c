#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <kerrighed.h>

void show_help()
{
	printf("Usage: ipcrestart <msg-queue ID> pathname\n");
}

int main(int argc, char *argv[])
{
	int r, msgqid, fd, _errno;

	if (argc != 3) {
		show_help();
		exit(EXIT_FAILURE);
	}

	msgqid = atoi(argv[1]);

	fd = open(argv[2], O_RDONLY, S_IRUSR|S_IWUSR);
	if (fd == -1) {
		_errno = errno;

		fprintf(stderr, "%s: %s\n", argv[2], strerror(_errno));
		exit(EXIT_FAILURE);
	}

	r = ipc_msgq_restart(msgqid, fd);

	_errno = errno;

	close(fd);

	if (r) {
		fprintf(stderr, "ipc_msgq_restart: %s\n", strerror(_errno));
		exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);
}
