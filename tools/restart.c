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
#include <libkrgcb.h>

#define CHKPT_DIR "/var/chkpt/"

long appid ;
int version ;
int flags = 0;
short foreground = 0;
short quiet = 0;
int root_pid = 0;

void show_help()
{
	printf ("Restart an application\nusage: restart [-h]"
		" [-f|-t] id version\n");
	printf ("  -h : This help\n");
	exit(1);
}

void parse_args(int argc, char *argv[])
{
	char c;
	int option_index = 0;
	char * short_options= "hftq";
	static struct option long_options[] =
		{
			{"help", no_argument, 0, 'h'},
			{"foreground", no_argument, 0, 'f'},
			{"replace-tty", no_argument, 0, 't'},
			{"quiet", no_argument, 0, 'q'},
			{0, 0, 0, 0}
		};

	while ((c = getopt_long(argc, argv, short_options,
				long_options, &option_index)) != -1) {
		switch (c)
		{
		case 'h':
			goto err;
			break;
		case 'f':
			foreground = 1;
			flags |= GET_RESTART_CMD_PTS;
			break;
		case 't':
			flags |= GET_RESTART_CMD_PTS;
			break;
		case 'q':
			quiet = 1;
			break;
		default:
			fprintf(stderr, "** unknown option\n");
		}
	}

	if (argc - optind < 2)
		goto err;

	appid = atol(argv[argc-2]);
	version = atoi(argv[argc-1]);

	return ;

err:
	show_help();
}

void show_error(int _errno)
{
	switch (_errno) {
	case E_CR_APPBUSY:
		fprintf(stderr, "restart: an application using appid %ld is "
			"already running\n", appid);
		break;
	case E_CR_PIDBUSY:
		fprintf(stderr, "restart: one PID (including session id or "
			"group id) of the application is already in use\n");
		break;
	case E_CR_BADDATA:
		fprintf(stderr, "restart: invalid checkpoint files "
			"(corrupted or wrong kernel version)\n");
		break;
	default:
		perror("restart");
	}
}

void relay_signal(int signum)
{
	if (root_pid)
		kill(root_pid, signum);
}

void wait_application_exits()
{
	int r, exit_status;
	pid_t child;
	struct sigaction action;

	action.sa_handler = &relay_signal;
	r = sigaction(SIGINT, &action, NULL);
	if (r)
		perror("sigaction");
	r = sigaction(SIGTERM, &action, NULL);
	if (r)
		perror("sigaction");

	do {
		child = wait(&exit_status);
		/* wait may be interrupted to handle a signal
		 * sent by the user */
		if (child <= 0 && child != -EINTR) {
			perror("wait");
			exit(EXIT_FAILURE);
		}
	} while (child == -EINTR);

	if (exit_status)
		exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
	int r = 0 ;

	if (get_nr_cpu() == -1)
	{
		fprintf (stderr, "%s: no kerrighed nodes found\n", argv[0]);
		exit(-1);
	}

	parse_args(argc, argv);

	if (!quiet)
		printf("Restarting application %ld (v%d) ...\n",
		       appid, version);

	r = application_restart(appid, version, flags);
	if (r < 0) {
		show_error(errno);
		exit(EXIT_FAILURE);
	}

	root_pid = r;

	r = cr_execute_restart_callbacks(appid);
	if (r)
		fprintf(stderr, "restart: error during callback execution");
	else if (!quiet)
		printf("Done\n");

	if (foreground)
		wait_application_exits();

	exit(EXIT_SUCCESS);
}
