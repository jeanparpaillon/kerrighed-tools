/*
 *  Copyright (C) 2001-2006, INRIA, Universite de Rennes 1, EDF.
 */

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

#define CHKPT_DIR "/var/chkpt"

typedef enum {
	ALL,
	CHECKPOINT,
	FREEZE,
	UNFREEZE,
} app_action_t;

short from_appid = 0;
short quiet = 0;
media_t media = DISK;
int sig = 0;
char * description = NULL;
app_action_t action = ALL;

void show_help(void)
{
	printf("Usage : \n"
	       "checkpoint [-h]:\t print this help\n"
	       "checkpoint [(-m|--media) disk|memory] [(-d|--description) description] [(-k|--kill)[signal]] "
	       "<pid> | -a <appid> :"
	       "\t checkpoint a running application\n"
	       "checkpoint -f|--freeze <pid>| -a <appid> :\t freeze an application\n"
	       "checkpoint -u|--unfreeze[=signal] <pid>| -a <appid> :\t unfreeze an application\n"
	       "checkpoint -c|--ckpt-only [(-m|--media) disk|memory] [(-d|--description) description] <pid> | -a <appid> :"
	       "\t checkpoint a frozen application\n"
	       );
}

void parse_args(int argc, char *argv[])
{
	char c;
	int option_index = 0;
	char * short_options= "hqam:cd:fu::k::";
	static struct option long_options[] = {
		{"help", no_argument, 0, 'h'},
		{"quiet", no_argument, 0, 'q'},
		{"from-appid", no_argument, 0, 'a'},
		{"media", required_argument, 0, 'm'},
		{"ckpt-only", no_argument, 0, 'c'},
		{"description", required_argument, 0, 'd'},
		{"freeze", no_argument, 0, 'f'},
		{"unfreeze", optional_argument, 0, 'u'},
		{"kill", optional_argument, 0, 'k'},
		{0, 0, 0, 0}
	};

	while ((c = getopt_long(argc, argv, short_options,
				long_options, &option_index)) != -1) {
		switch (c) {
		case 'h':
			show_help();
			return;
		case 'q':
			quiet=1;
			break;
		case 'a':
			from_appid=1;
			break;
		case 'm':
			if (strcmp(optarg, "DISK") == 0)
				media = DISK;
			else if (strcmp(optarg, "MEMORY") == 0)
				media = MEMORY;
			else printf("Warning: Unknown type of media\n");
			break;
		case 'd':
			description = optarg;
			break;
		case 'c':
			action = CHECKPOINT;
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
		default:
			printf("Warning: Option %c "
			       "not implemented\n", c);
			break;
		}
	}
}


void check_environment(void)
{
	struct stat buffer;
	int status;

	/* is Kerrighed launched ? */
	if (get_nr_cpu() == -1)
	{
		fprintf(stderr, "no kerrighed nodes found\n");
		exit(-EPERM);
	}

	/* /var/chkpt exists ? */
	status = stat(CHKPT_DIR, &buffer);
	if (status) {
		perror(CHKPT_DIR);
		exit(-ENOENT);
	}
}

int write_description(char *description,
		      checkpoint_infos_t *infos)
{
	FILE* fd;
	char path[256];
	time_t date = time(NULL);

	if (!description)
		description = "No description";

	printf("Identifier: %ld\n"
	       "Version: %d\n"
	       "Description: %s\n"
	       "Date: %s\n",
	       infos->app_id,
	       infos->chkpt_sn,
	       description,
	       ctime(&date));

	// need to write it in a file
	sprintf(path, "%s/%ld/v%d/description.txt", CHKPT_DIR,
		infos->app_id, infos->chkpt_sn);

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
		infos->app_id,
		infos->chkpt_sn,
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

void clean_checkpoint_dir(checkpoint_infos_t *infos)
{
	DIR *dir;
	struct dirent *ent;
	char path[256];
	int r;

	if (!infos->chkpt_sn)
		return;

	/* to refresh NFS cache... */
	dir = opendir(CHKPT_DIR);
	if (!dir) {
		perror("opendir");
		return;
	}
	closedir(dir);

	sprintf(path, "%s/%ld/", CHKPT_DIR, infos->app_id);
	dir = opendir(path);
	if (!dir) {
		perror("opendir");
		return;
	}
	closedir(dir);

	sprintf(path, "%s/%ld/v%d/", CHKPT_DIR,
		infos->app_id, infos->chkpt_sn);

	/* remove the file and directory */
	dir = opendir(path);
	if (!dir) {
		perror("opendir");
		return;
	}

	while ((ent = readdir(dir)) != NULL) {
		if (ent->d_type == DT_REG) {
			sprintf(path, "%s/%ld/v%d/%s", CHKPT_DIR,
				infos->app_id, infos->chkpt_sn, ent->d_name);
			r = remove(path);
			if (r)
				perror("remove");
		}
	}
	closedir(dir);

	sprintf(path, "%s/%ld/v%d/", CHKPT_DIR,
		infos->app_id, infos->chkpt_sn);
	r = remove(path);
	if (r)
		perror("remove");

	sprintf(path, "%s/%ld/", CHKPT_DIR, infos->app_id);
	remove(path);
}

int checkpoint_app(long pid, short _quiet)
{
	int r;

	checkpoint_infos_t infos;
	if (from_appid) {
		if (!_quiet)
			printf("Checkpointing application %ld...\n", pid);

		infos = application_checkpoint_from_appid(media, pid);
	} else {
		if (!_quiet)
			printf("Checkpointing application in which "
			       "process %d is involved...\n", (pid_t)pid);
		infos = application_checkpoint_from_pid(media, (pid_t)pid);
	}

	r = infos.result;

	if (!r)
		write_description(description, &infos);
	else {
		show_error(errno);
		clean_checkpoint_dir(&infos);
	}

	return r;
}

int freeze_app(long pid, int _quiet)
{
	int r;
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

	return r;
}

int unfreeze_app(long pid, int signal, short _quiet)
{
	int r;
	if (from_appid) {
		if (!_quiet)
			printf("Unfreezing application %ld...\n", pid);

		r = application_unfreeze_from_appid(pid, signal);
	} else {
		if (!_quiet)
			printf("Unfreezing application in which "
			       "process %d is involved...\n", (pid_t)pid);

		r = application_unfreeze_from_pid((pid_t)pid, signal);
	}

	if (r)
		show_error(errno);

	return r;
}

int freeze_checkpoint_unfreeze(long pid, int signal, short _quiet)
{
	int r;

	r = freeze_app(pid, _quiet);
	if (r)
		goto err_freeze;
	r = checkpoint_app(pid, _quiet);
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

	/* Check environment */
	check_environment();

	/* Manage options with getopt */
	if (argc == 1) {
		show_help();
		return 0;
	}
	parse_args(argc, argv);

	/* get the pid */
	pid = atol( argv[argc-1] );
	if (pid < 2) {
		r = -EINVAL;
		goto exit;
	}

	switch (action) {
	case CHECKPOINT:
		r = checkpoint_app(pid, quiet);
		break;
	case FREEZE:
		r = freeze_app(pid, quiet);
		break;
	case UNFREEZE:
		r = unfreeze_app(pid, sig, quiet);
		break;
	case ALL:
		r = freeze_checkpoint_unfreeze(pid, sig, quiet);
		break;
	}

exit:
	if (r)
		exit(EXIT_FAILURE);

	exit(EXIT_SUCCESS);
}
