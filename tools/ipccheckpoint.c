#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <getopt.h>
#include <kerrighed.h>

#include <config.h>

enum ipctype {
	UNDEF,
	MSG,
	SEM,
	SHM
};

enum ipctype ipc_type = UNDEF;
int ipcid;

void version(char * program_name)
{
	printf("\
%s %s\n\
Copyright (C) 2010 Kerlabs.\n\
This is free software; see source for copying conditions. There is NO\n\
warranty; not even for MERCHANBILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\
\n", program_name, VERSION);
}

void show_help(char * program_name)
{
	printf("Usage: %s [-h|--help] [-v|--version] {-q|-s|-m} <IPC ID> pathname\n"
	       "  -h|--help       Show this information and exit\n"
	       "  -v|--version    Show version informations and exit\n"
	       "  -q|--queue      for a message queue\n"
	       "  -s|--semaphore  for a semaphore array\n"
	       "  -m|--memory     for a shared memory segment\n",
	       program_name);
}

void parse_args(int argc, char *argv[])
{
	char c;
	int option_index = 0;
	char * short_options= "hq:s:m:";
	static struct option long_options[] = {
		{"help", no_argument, 0, 'h'},
		{"version", no_argument, 0, 'v'},
		{"queue", required_argument, 0, 'q'},
		{"semaphore", required_argument, 0, 's'},
		{"memory", required_argument, 0, 'm'},
		{0, 0, 0, 0}
	};

	while ((c = getopt_long(argc, argv, short_options,
				long_options, &option_index)) != -1) {
		switch (c) {
		case 'h':
			show_help(argv[0]);
			exit(EXIT_SUCCESS);
		case 'v':
			version(argv[0]);
			exit(EXIT_SUCCESS);
		case 'q':
			ipc_type = MSG;
			ipcid = atoi(optarg);
			break;
		case 's':
			ipc_type = SEM;
			ipcid = atoi(optarg);
			break;
		case 'm':
			ipc_type = SHM;
			ipcid = atoi(optarg);
			break;
		default:
			show_help(argv[0]);
			exit(EXIT_FAILURE);
			break;
		}
	}
}

int msgq_checkpoint(int ipcid, int fd)
{
	int r, _errno;

	r = ipc_msgq_checkpoint(ipcid, fd);

	if (r) {
		_errno = errno;
		fprintf(stderr, "ipc_msgq_checkpoint: %s\n", strerror(_errno));
	}
	return r;
}

int sem_checkpoint(int ipcid, int fd)
{
	int r, _errno;

	r = ipc_sem_checkpoint(ipcid, fd);

	if (r) {
		_errno = errno;
		fprintf(stderr, "ipc_sem_checkpoint: %s\n", strerror(_errno));
	}
	return r;
}

int shm_checkpoint(int ipcid, int fd)
{
	int r, _errno;

	r = ipc_shm_checkpoint(ipcid, fd);

	if (r) {
		_errno = errno;
		fprintf(stderr, "ipc_shm_checkpoint: %s\n", strerror(_errno));
	}
	return r;
}

int main(int argc, char *argv[])
{
	int r, fd, _errno;

	parse_args(argc, argv);

	if (ipc_type == UNDEF
	    || argc - optind != 1) {
		show_help(argv[0]);
		exit(EXIT_FAILURE);
	}

	fd = open(argv[optind], O_CREAT|O_EXCL|O_WRONLY, S_IRUSR|S_IWUSR);
	if (fd == -1) {
		_errno = errno;

		fprintf(stderr, "%s: %s\n", argv[optind], strerror(_errno));
		exit(EXIT_FAILURE);
	}

	switch (ipc_type) {
	case MSG:
		r = msgq_checkpoint(ipcid, fd);
		break;
	case SEM:
		r = sem_checkpoint(ipcid, fd);
		break;
	case SHM:
		r = shm_checkpoint(ipcid, fd);
		break;
	default:
		show_help(argv[0]);
		exit(EXIT_FAILURE);
	}

	close(fd);

	if (r) {
		unlink(argv[optind]);
		exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);
}
