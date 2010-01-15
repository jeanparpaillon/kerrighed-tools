/*
 *  Copyright (C) 2001-2006, INRIA, Universite de Rennes 1, EDF.
 */

#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <dirent.h>
#include <getopt.h>
#include <time.h>
#include <kerrighed.h>
#include <libkrgcb.h>

#define CHKPT_DIR "/var/chkpt"

typedef enum {
	ALL,
	CHECKPOINT,
	FREEZE,
	UNFREEZE,
} app_action_t;

short from_appid = 0;
short quiet = 0;
short no_callbacks = 0;
int sig = 0;
int flags = 0;
char * description = NULL;
app_action_t action = ALL;

void show_help(void)
{
	printf("Usage : \n"
	       "checkpoint [-h]:\t print this help\n"
	       "checkpoint [-b --no-callbacks] [(-d|--description) description] [(-k|--kill)[signal]] "
	       "<pid> | -a <appid> DIRECTORY:"
	       "\t checkpoint a running application and store checkpoint in directory DIRECTORY\n"
	       "checkpoint  [-b --no-callbacks] -f|--freeze <pid>| -a <appid> :\t freeze an application\n"
	       "checkpoint  [-b --no-callbacks] -u|--unfreeze[=signal] <pid>| -a <appid> :\t unfreeze an application\n"
	       "checkpoint -c|--ckpt-only [(-d|--description) description] <pid> | -a <appid> :"
	       "\t checkpoint a frozen application and store checkpoint in directory DIRECTORY\n"
	       );
}

void parse_args(int argc, char *argv[])
{
	char c;
	int option_index = 0;
	char * short_options= "hqacd:bfu::k::ip";
	static struct option long_options[] = {
		{"help", no_argument, 0, 'h'},
		{"quiet", no_argument, 0, 'q'},
		{"from-appid", no_argument, 0, 'a'},
		{"ckpt-only", no_argument, 0, 'c'},
		{"description", required_argument, 0, 'd'},
		{"no-callbacks", no_argument, 0, 'b'},
		{"freeze", no_argument, 0, 'f'},
		{"unfreeze", optional_argument, 0, 'u'},
		{"kill", optional_argument, 0, 'k'},
		{"ignore-unsupported-files", no_argument, 0, 'i'},
		{"create-directory", no_argument, 0, 'p'},
		{0, 0, 0, 0}
	};

	while ((c = getopt_long(argc, argv, short_options,
				long_options, &option_index)) != -1) {
		switch (c) {
		case 'h':
			show_help();
			exit(EXIT_SUCCESS);
			break;
		case 'q':
			quiet=1;
			break;
		case 'a':
			from_appid=1;
			break;
		case 'd':
			description = optarg;
			break;
		case 'c':
			action = CHECKPOINT;
			break;
		case 'b':
			no_callbacks = 1;
			break;
		case 'f':
			action = FREEZE;
			break;
		case 'u':
			action = UNFREEZE;
			if (optarg)
				sig = atoi(optarg);
			break;
		case 'k':
			if (optarg)
				sig = atoi(optarg);
			else
				sig = 15;
			break;
		case 'i':
			flags |= CKPT_W_UNSUPPORTED_FILE;
			break;
		case 'p':
			flags |= CREATE_DIR;
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

int write_description(char *description,
		      struct checkpoint_info *info,
		      const char* checkpoint_dir,
		      short _quiet)
{
	FILE* fd;
	char path[PATH_MAX];
	time_t date = time(NULL);

	if (!description)
		description = "No description";

	if (!_quiet)
		printf("Identifier: %ld\n"
		       "Version: %d\n"
		       "Description: %s\n"
		       "Date: %s\n",
		       info->app_id,
		       info->chkpt_sn,
		       description,
		       ctime(&date));

	/* need to write it in a file */
	snprintf(path, PATH_MAX, "%s/description.txt", checkpoint_dir);

	fd = fopen(path, "a");
	if (!fd) {
		perror(path);
		return -1;
	}

	fprintf(fd,
		"Identifier: %ld\n"
		"Version: %d\n"
		"Description: %s\n"
		"Date: %ld\n",
		info->app_id,
		info->chkpt_sn,
		description,
		date);
	fclose(fd);

	return 0;
}

void show_error(int _errno)
{
	switch (_errno) {
	case E_CR_TASKDEAD:
		fprintf(stderr, "checkpoint: can not checkpoint an application with dead/zombie process\n");
		break;
	default:
		perror("checkpoint");
		break;
	}
}

void clean_checkpoint_dir(struct checkpoint_info *info,
			  const char *checkpoint_dir)
{
	/* we cannot clean the checkpoint dir easily since it has been
	 *  given by the user.
	 * There may be some files unrelated with current checkpoint.
	 */

	fprintf(stderr,
		"WARNING: checkpoint has failed. Some files may be left"
		"in the directory %s.\n", checkpoint_dir);
}

int checkpoint_app(long pid, int flags, const char *checkpoint_dir,
		   short _quiet)
{
	int r;

	struct checkpoint_info info;
	if (from_appid) {
		if (!_quiet)
			printf("Checkpointing application %ld...\n", pid);

		info = application_checkpoint_from_appid(pid, flags,
							 checkpoint_dir);
	} else {
		if (!_quiet)
			printf("Checkpointing application in which "
			       "process %d is involved...\n", (pid_t)pid);
		info = application_checkpoint_from_pid((pid_t)pid, flags,
						       checkpoint_dir);
	}

	r = info.result;

	if (!r)
		write_description(description, &info, checkpoint_dir, _quiet);
	else {
		show_error(errno);
		clean_checkpoint_dir(&info, checkpoint_dir);
	}

	return r;
}

int freeze_app(long pid, int _quiet)
{
	int r;

	if (!no_callbacks) {
		r = cr_execute_chkpt_callbacks(pid, from_appid);
		if (r) {
			fprintf(stderr, "checkpoint: error during callback"
				" execution\n");
			goto err;
		}
	}

	if (from_appid) {
		if (!_quiet)
			printf("Freezing application %ld...\n", pid);

		r = application_freeze_from_appid(pid);
	} else {
		if (!_quiet)
			printf("Freezing application in which "
			       "process %d is involved...\n", (pid_t)pid);

		r = application_freeze_from_pid((pid_t)pid);
	}

	if (r)
		show_error(errno);

err:
	return r;
}

int unfreeze_app(long pid, int signal, short _quiet)
{
	int r;

	if (!no_callbacks) {
		r = cr_execute_continue_callbacks(pid, from_appid);
		if (r) {
			fprintf(stderr, "checkpoint: error during callback"
				" execution\n");
			goto err;
		}
	}

	if (from_appid) {
		if (!_quiet)
			printf("Unfreezing application %ld...\n", pid);

		r = application_unfreeze_from_appid(pid, signal);
		if (r)
			goto err_show;
	} else {
		if (!_quiet)
			printf("Unfreezing application in which "
			       "process %d is involved...\n", (pid_t)pid);

		r = application_unfreeze_from_pid((pid_t)pid, signal);
		if (r)
			goto err_show;
	}

err_show:
	if (r)
		show_error(errno);

err:
	return r;
}

int freeze_checkpoint_unfreeze(long pid, int flags, const char* checkpoint_dir,
			       int signal, short _quiet)
{
	int r;

	r = freeze_app(pid, _quiet);
	if (r)
		goto err_freeze;
	r = checkpoint_app(pid, flags, checkpoint_dir, _quiet);
	if (r)
		goto err_chkpt;
	r = unfreeze_app(pid, signal, _quiet);

err_freeze:
	return r;

err_chkpt:
	/* silently unfreezing without any signal*/
	unfreeze_app(pid, 0, 1);
	return r;
}

void mkdirp(const char *dir)
{
	krg_cap_t caps;
	char cmd[4096];
	int r;

	r = krg_capget(&caps);
	if (r) {
		fprintf(stderr,
			"Fail to get Kerrighed capabilities "
			"of current process (%d)\n", getpid());
		exit(EXIT_FAILURE);
	}

	caps.krg_cap_effective &= ~(1 << CAP_DISTANT_FORK);
	caps.krg_cap_inheritable_effective
		&= ~((1 << CAP_DISTANT_FORK) | (1 << CAP_CAN_MIGRATE));

	r = krg_capset(&caps);
	if (r) {
		fprintf(stderr, "Fail to set Kerrighed capabilities "
			"of current process (%d)\n", getpid());
		exit(EXIT_FAILURE);
	}

	sprintf(cmd, "mkdir -p %s", dir);

	system(cmd);

	return;
}

int main(int argc, char *argv[])
{
	int r = 0;
	long pid = -1;
	char *checkpoint_dir;

	/* Check environment */
	check_environment();

	/* Manage options with getopt */
	parse_args(argc, argv);

	if (action == ALL || action == CHECKPOINT) {
		if (argc - optind != 2) {
			show_help();
			r = -EINVAL;
			goto exit;
		}

		mkdirp(argv[optind+1]);

		checkpoint_dir = canonicalize_file_name(argv[optind+1]);
		if (!checkpoint_dir) {
			r = errno;
			perror(argv[optind+1]);
			goto exit;
		}

	} else if (argc - optind != 1) {
		show_help();
		r = -EINVAL;
		goto exit;
	}

	/* get the pid */
	pid = atol( argv[optind] );
	if (pid < 2) {
		r = -EINVAL;
		goto exit;
	}

	switch (action) {
	case CHECKPOINT:
		r = checkpoint_app(pid, flags, checkpoint_dir, quiet);
		break;
	case FREEZE:
		r = freeze_app(pid, quiet);
		break;
	case UNFREEZE:
		r = unfreeze_app(pid, sig, quiet);
		break;
	case ALL:
		r = freeze_checkpoint_unfreeze(pid, flags, checkpoint_dir, sig,
					       quiet);
		break;
	}

	free(checkpoint_dir);

exit:
	if (r)
		exit(EXIT_FAILURE);

	exit(EXIT_SUCCESS);
}
