/*
 *  Copyright (C) 2001-2006, INRIA, Universite de Rennes 1, EDF.
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <getopt.h>
#include <time.h>
#include <kerrighed.h>

#define CHKPT_DIR "/var/chkpt/"

long appid ;
int version ;
media_t media;
int flags = 0;
short foreground = 0;

void show_help()
{
	printf ("Restart an application\nusage: restart [-h]"
		" [--media DISK|MEMORY] [-f|-t] id version\n");
	printf ("  -h : This help\n");
	exit(1);
}

void parse_args(int argc, char *argv[])
{
	char c;
	int option_index = 0;
	char * short_options= "hm:ft";
	static struct option long_options[] =
		{
			{"help", no_argument, 0, 'h'},
			{"foreground", no_argument, 0, 'f'},
			{"replace-tty", no_argument, 0, 't'},
			{"media", required_argument, 0, 'm'},
			{0, 0, 0, 0}
		};

	while ((c = getopt_long(argc, argv, short_options,
				long_options, &option_index)) != -1) {
		switch (c)
		{
		case 'h':
			goto err;
			break;
		case 'm':
			if (strcmp(optarg, "DISK") == 0)
				media = DISK;
			else if (strcmp(optarg, "MEMORY") == 0)
				media = MEMORY;
			else printf("Warning: Unknown type of media\n");
			break;
		case 'f':
			foreground = 1;
			flags |= GET_RESTART_CMD_PTS;
			break;
		case 't':
			flags |= GET_RESTART_CMD_PTS;
			break;
		default:
			printf("** unknown option\n");
		}
	}

	if (argc - optind < 2)
		goto err;

	appid = atoi(argv[argc-2]);
	version = atoi(argv[argc-1]);

	return ;

err:
	show_help();
}

int main(int argc, char *argv[])
{
	int r = 0 ;

	if (get_nr_cpu() == -1)
	{
		printf ("%s: no kerrighed nodes found\n", argv[0]);
		exit(-1);
	}

	parse_args(argc, argv);

	printf("Restarting application %ld (v%d) ...\n", appid, version);

	r = application_restart (media, appid, version, flags);
	if (r != 0)
		r = errno;

	switch (r) {
	case 0:
		printf("Done\n");
		break;
	case E_CR_APPBUSY:
		fprintf(stderr, "restart: an application using appid %ld is already "
		       "running\n", appid);
		break;
	case E_CR_PIDBUSY:
		fprintf(stderr, "restart: one PID of the application is already in use\n");
		break;
	case E_CR_BADDATA:
		fprintf(stderr, "restart: invalid checkpoint files "
			"(corrupted or wrong kernel version)\n");
		break;
	default:
		perror("restart");
	}

	if (!r && foreground) {
		int exit_status;
		pid_t child = wait(&exit_status);

		if (child <= 0)
			perror("wait");

		if (exit_status)
			exit(EXIT_FAILURE);
	}

	return r;
}
