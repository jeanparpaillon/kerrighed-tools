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

struct cr_subst_files_array substitution;

int array_size = 0;
const int ARRAY_SIZE_INC = 32;

void show_help()
{
	printf ("Restart an application\nusage: restart [-h]"
		" [-f|-t] id version\n");
	printf ("  -h : This help\n");
	exit(1);
}

int inc_substitution_array_size(struct cr_subst_files_array *subst_array,
				int array_size)
{
	struct cr_subst_file *files;

	if (subst_array->nr + 1 > array_size) {
		array_size = array_size + ARRAY_SIZE_INC;

		files = realloc(subst_array->files,
				array_size * sizeof(struct cr_subst_file));

		if (!files) {
			perror("realloc");
			return -ENOMEM;
		}

		subst_array->files = files;
	}

	subst_array->nr++;

	return array_size;
}

int parse_file_substitution(char *fileinfo_str)
{
	int i, r, fd;

	i = substitution.nr;

	r = inc_substitution_array_size(&substitution, array_size);
	if (r < 0)
		goto error;

	array_size = r;

	r = sscanf(fileinfo_str, "%a[0-9A-F],%d",
		   &substitution.files[i].file_id, &fd);
	if (r == 2) {
		substitution.files[i].fd = fd;
		r = 0;
	} else
		r = -EINVAL;

error:
	return r;
}

void clean_file_substitution(struct cr_subst_files_array *subst_array)
{
	unsigned int i;

	for (i = 0; i < subst_array->nr; i++)
		free(subst_array->files[i].file_id);
}

void parse_args(int argc, char *argv[])
{
	char c;
	int r, option_index = 0;
	char * short_options= "hfts:q";
	static struct option long_options[] =
		{
			{"help", no_argument, 0, 'h'},
			{"foreground", no_argument, 0, 'f'},
			{"tty", no_argument, 0, 't'},
			{"substitute-file", required_argument, 0, 's'},
			{"quiet", no_argument, 0, 'q'},
			{0, 0, 0, 0}
		};

	r = 0;

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
		case 's':
			r = parse_file_substitution(optarg);
			break;
		case 'q':
			quiet = 1;
			break;
		default:
			show_help();
			exit(EXIT_FAILURE);
			break;
		}

		if (r) {
			fprintf(stderr, "restart: fail to parse args: %s\n", strerror(-r));
			exit(EXIT_FAILURE);
		}
	}

	if (argc - optind != 2) {
		show_help();
		exit(EXIT_FAILURE);
	}

	appid = atol(argv[optind]);
	version = atoi(argv[optind+1]);
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

	if (get_nr_nodes() == -1)
	{
		fprintf (stderr, "%s: no kerrighed nodes found\n", argv[0]);
		exit(-1);
	}

	parse_args(argc, argv);

	if (!quiet)
		printf("Restarting application %ld (v%d) ...\n",
		       appid, version);

	r = application_restart(appid, version, flags, &substitution);
	if (r < 0) {
		show_error(errno);
		goto exit;
	}

	root_pid = r;

	r = cr_execute_restart_callbacks(appid);
	if (r) {
		fprintf(stderr, "restart: error during callback execution\n");
		goto exit;
	}

	r = application_unfreeze_from_appid(appid, 0);
	if (r) {
		perror("restart");
		fprintf(stderr, "restart: fail to unfreeze the application\n");
		goto exit;
	}

	if (!quiet)
		printf("Done\n");

	if (foreground)
		wait_application_exits();

exit:
	clean_file_substitution(&substitution);

	if (r)
		exit(EXIT_FAILURE);

	exit(EXIT_SUCCESS);
}
