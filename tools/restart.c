/*
 *  Copyright (C) 2001-2006, INRIA, Universite de Rennes 1, EDF.
 */

#define _GNU_SOURCE
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

int flags = 0;
short foreground = 0;
short quiet = 0;
int root_pid = 0;

void show_help()
{
	printf ("Restart an application\nusage: restart [-h]"
		" [-f|-t] directory\n");
	printf ("  -h : This help\n");
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
			show_help();
			exit(EXIT_SUCCESS);
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
			show_help();
			exit(EXIT_FAILURE);
			break;
		}
	}
}

void check_environment(void)
{
	/* is Kerrighed launched ? */
	if (get_nr_nodes() == -1)
	{
		fprintf(stderr, "no kerrighed nodes found\n");
		exit(-EPERM);
	}
}

void show_error(long appid, int _errno)
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
		fprintf(stderr, "restart: %s\n", strerror(_errno));
		break;
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
	long appid;
	int r = 0;
	char *checkpoint_dir;

	/* Check environment */
	check_environment();

	/* Manage options with getopt */
	parse_args(argc, argv);

	if (argc - optind != 1) {
		show_help();
		goto exit;
 	}

	checkpoint_dir = canonicalize_file_name(argv[optind]);
	if (!checkpoint_dir) {
		r = errno;
		perror(argv[optind]);
		goto exit_failure;
	}

	if (!quiet)
		printf("Restarting application from %s...\n",
			checkpoint_dir);

	r = application_restart(&appid, checkpoint_dir, flags);
	if (r < 0) {
		show_error(appid, errno);
		goto exit_failure;
	}

	root_pid = r;

	r = cr_execute_restart_callbacks(appid);
	if (r) {
		fprintf(stderr, "restart: error during callback execution\n");
		goto exit_failure;
	}

	r = application_unfreeze_from_appid(appid, 0);
	if (r) {
		perror("restart");
		fprintf(stderr, "restart: fail to unfreeze the application\n");
		goto exit_failure;
	}

	if (!quiet)
		printf("Application %ld has been successfully restarted\n",
		       appid);

	if (foreground)
		wait_application_exits();

	free(checkpoint_dir);

	exit(EXIT_SUCCESS);

exit_failure:
	free(checkpoint_dir);
exit:
	exit(EXIT_FAILURE);
}
