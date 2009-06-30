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

#include <getopt.h>
#include <time.h>
#include <kerrighed.h>

#define CHKPT_DIR "/var/chkpt/"

typedef enum {
	ALL,
	CHECKPOINT,
	FREEZE,
	UNFREEZE,
} app_action_t;

short from_appid = 0;
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
	char * short_options= "ham:cd:fu::k::";
	static struct option long_options[] = {
		{"help", no_argument, 0, 'h'},
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

int checkpoint_app(long pid)
{
	int r;

	checkpoint_infos_t infos;
	if (from_appid) {
		printf("Checkpointing application %ld...\n", pid);
		infos = application_checkpoint_from_appid(media, pid);
	} else {
		printf("Checkpointing application in which "
		       "process %d is involved...\n", (pid_t)pid);
		infos = application_checkpoint_from_pid(media, (pid_t)pid);
	}

	r = infos.result;

	if (!r)
		write_description(description, &infos);

	return r;
}

int freeze_app(long pid)
{
	int r;
	if (from_appid) {
		printf("Freezing application %ld...\n", pid);
		r = application_freeze_from_appid(pid);
	} else {
		printf("Freezing application in which "
		       "process %d is involved...\n", (pid_t)pid);
		r = application_freeze_from_pid((pid_t)pid);
	}

	return r;
}

int unfreeze_app(long pid)
{
	int r;
	if (from_appid) {
		printf("Unfreezing application %ld...\n", pid);
		r = application_unfreeze_from_appid(pid, sig);
	} else {
		printf("Unfreezing application in which "
		       "process %d is involved...\n", (pid_t)pid);
		r = application_unfreeze_from_pid((pid_t)pid, sig);
	}

	return r;
}

int freeze_checkpoint_unfreeze(long pid)
{
	int r;

	r = freeze_app(pid);
	if (r)
		goto out;
	r = checkpoint_app(pid);
	if (r)
		goto out;
	r = unfreeze_app(pid);

out:
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
		r = checkpoint_app(pid);
		break;
	case FREEZE:
		r = freeze_app(pid);
		break;
	case UNFREEZE:
		r = unfreeze_app(pid);
		break;
	case ALL:
		r = freeze_checkpoint_unfreeze(pid);
	}

	if (r!=0)
		r = errno;

exit:
	switch (r) {
	case 0:
		break;
	case E_CR_TASKDEAD:
		fprintf(stderr, "checkpoint: can not checkpoint an application with dead/zombie process\n");
		break;
	default:
		perror("checkpoint");
		break;
	}

	return r;
}
