/*
 *  Copyright (C) 2001-2006, INRIA, Universite de Rennes 1, EDF.
 *  Copyright (c) 2010, Kerlabs
 *
 * Authors:
 *    Matthieu Fertre <matthieu.fertre@kerlabs.com>
 *    Jean Parpaillon <jean.parpaillon@kerlabs.com>
 */

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <dirent.h>
#include <getopt.h>
#include <time.h>
#include <kerrighed.h>
#include <libkrgcb.h>

#include <config.h>

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

void version(char * program_name)
{
	printf("\
%s %s\n\
Copyright (C) 2001-2006, INRIA, Universite de Rennes 1, EDF.\n\
Copyright (C) 2010 Kerlabs.\n\
This is free software; see source for copying conditions. There is NO\n\
warranty; not even for MERCHANBILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\
\n", program_name, VERSION);
}

void show_help(char * program_name)
{
	printf("Usage: %s [options] <pid>\n"
	       "\n"
	       "Mutually Exclusive Options:\n"
	       "  Without any of these options, freeze and checkpoint the application.\n"
	       "  -f|--freeze             Freeze the application\n"
	       "  -u|--unfreeze [signal]  Unfreeze the application\n"
	       "  -c|--ckpt-only          Checkpoint a frozen application\n"
	       "  -k|--kill [signal]      Send a signal to the application, after checkpointing and before unfreezing\n"
	       "\n"
	       "General Options:\n"
	       "  -h|--help               Display this information and exit\n"
	       "  -v|--version            Display version informations and exit\n"
	       "  -q|--quiet              Be less verbose\n"
	       "  -b|--no-callbacks       Do not execute callbacks\n"
	       "  -d|--description        Associate a description with the checkpoint\n"
	       "  -a|--appid              Use <pid> as an application identifier rather than a process identifier\n"
	       "  -i|--ignore-unsupported-files\n"
	       "                          Allow to checkpoint application with open files of unsupported type\n",
	       program_name);
}

void parse_args(int argc, char *argv[])
{
	char c;
	int option_index = 0;
	char * short_options= "hqacd:bfu::k::i";
	static struct option long_options[] = {
		{"help", no_argument, 0, 'h'},
		{"version", no_argument, 0, 'v'},
		{"quiet", no_argument, 0, 'q'},
		{"from-appid", no_argument, 0, 'a'},
		{"ckpt-only", no_argument, 0, 'c'},
		{"description", required_argument, 0, 'd'},
		{"no-callbacks", no_argument, 0, 'b'},
		{"freeze", no_argument, 0, 'f'},
		{"unfreeze", optional_argument, 0, 'u'},
		{"kill", optional_argument, 0, 'k'},
		{"ignore-unsupported-files", no_argument, 0, 'i'},
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
		default:
			show_help(argv[0]);
			exit(EXIT_FAILURE);
			break;
		}
	}
}


void check_environment(void)
{
	struct stat buffer;
	int ret;

	/* is Kerrighed launched ? */
	ret = krg_check_hotplug();
	if (ret) {
		perror("Kerrighed is not started");
		exit(EXIT_FAILURE);
	}

	/* Does /var/chkpt exist ? */
	ret = stat(CHKPT_DIR, &buffer);
	if (ret) {
		perror(CHKPT_DIR);
		exit(EXIT_FAILURE);
	}
}

int write_description(char *description,
		      struct checkpoint_info *info,
		      short _quiet)
{
	FILE* fd;
	char path[256];
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

	// need to write it in a file
	sprintf(path, "%s/%ld/v%d/description.txt", CHKPT_DIR,
		info->app_id, info->chkpt_sn);

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

void clean_checkpoint_dir(struct checkpoint_info *info)
{
	DIR *dir;
	struct dirent *ent;
	char path[256];
	int r;

	if (!info->chkpt_sn)
		return;

	/* to refresh NFS cache... */
	dir = opendir(CHKPT_DIR);
	if (!dir) {
		perror("opendir");
		return;
	}
	closedir(dir);

	sprintf(path, "%s/%ld/", CHKPT_DIR, info->app_id);
	dir = opendir(path);
	if (!dir) {
		perror("opendir");
		return;
	}
	closedir(dir);

	sprintf(path, "%s/%ld/v%d/", CHKPT_DIR,
		info->app_id, info->chkpt_sn);

	/* remove the file and directory */
	dir = opendir(path);
	if (!dir) {
		perror("opendir");
		return;
	}

	while ((ent = readdir(dir)) != NULL) {
		if (ent->d_type == DT_REG) {
			sprintf(path, "%s/%ld/v%d/%s", CHKPT_DIR,
				info->app_id, info->chkpt_sn, ent->d_name);
			r = remove(path);
			if (r)
				perror("remove");
		}
	}
	closedir(dir);

	sprintf(path, "%s/%ld/v%d/", CHKPT_DIR,
		info->app_id, info->chkpt_sn);
	r = remove(path);
	if (r)
		perror("remove");

	sprintf(path, "%s/%ld/", CHKPT_DIR, info->app_id);
	remove(path);
}

int checkpoint_app(long pid, int flags, short _quiet)
{
	int r;

	struct checkpoint_info info;
	if (from_appid) {
		if (!_quiet)
			printf("Checkpointing application %ld...\n", pid);

		info = application_checkpoint_from_appid(pid, flags);
	} else {
		if (!_quiet)
			printf("Checkpointing application in which "
			       "process %d is involved...\n", (pid_t)pid);
		info = application_checkpoint_from_pid((pid_t)pid, flags);
	}

	r = info.result;

	if (!r)
		write_description(description, &info, _quiet);
	else {
		show_error(errno);
		clean_checkpoint_dir(&info);
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

int freeze_checkpoint_unfreeze(long pid, int flags, int signal, short _quiet)
{
	int r;

	r = freeze_app(pid, _quiet);
	if (r)
		goto err_freeze;
	r = checkpoint_app(pid, flags, _quiet);
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

int main(int argc, char *argv[])
{
	int r = 0;
	long pid = -1;

	/* Manage options with getopt */
	parse_args(argc, argv);

	if (argc - optind != 1) {
		show_help(argv[0]);
		exit(EXIT_FAILURE);
	}

	/* Check environment */
	check_environment();

	/* get the pid */
	pid = atol( argv[optind] );
	if (pid < 2) {
		r = -EINVAL;
		goto exit;
	}

	switch (action) {
	case CHECKPOINT:
		r = checkpoint_app(pid, flags, quiet);
		break;
	case FREEZE:
		r = freeze_app(pid, quiet);
		break;
	case UNFREEZE:
		r = unfreeze_app(pid, sig, quiet);
		break;
	case ALL:
		r = freeze_checkpoint_unfreeze(pid, flags, sig, quiet);
		break;
	}

exit:
	if (r)
		exit(EXIT_FAILURE);

	exit(EXIT_SUCCESS);
}
