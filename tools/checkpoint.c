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
	       "\t checkpoint a running application and store checkpoint in DIRECTORY\n"
	       "checkpoint  [-b --no-callbacks] -f|--freeze <pid>| -a <appid> :\t freeze an application\n"
	       "checkpoint  [-b --no-callbacks] -u|--unfreeze[=signal] <pid>| -a <appid> :\t unfreeze an application\n"
	       "checkpoint -c|--ckpt-only [(-d|--description) description] <pid> | -a <appid> :"
	       "\t checkpoint a frozen application and store checkpoint in DIRECTORY\n"
	       );
}

void lookup_parent_directory(const char *checkpoint_dir)
{
	int error, len, fd;
	struct stat statbuf;
	char *copy, *delim, *tmp;

	copy = strdup(checkpoint_dir);
	if (!copy)
		return;

	tmp = copy;

	while (1) {
		delim = strchr(tmp, '/');
		if (!delim)
			goto end_of_path;

		*delim = '\0';
		len = strlen(copy);
		if (len) {
/* 			error = stat(copy, &statbuf); */
/* 			if (error) { */
/* 				fprintf(stderr, "stat %s: %d\n", copy, error); */
/* 				goto out; */
/* 			} */

			/* chmod empties the cache */
			error = chmod(copy, S_IRUSR | S_IWUSR | S_IXUSR);
			if (error) {
				fprintf(stderr, "chmod %s: %d\n", copy, error);
				goto out;
			}

			fd = open(copy, O_RDONLY|O_NONBLOCK|O_DIRECTORY|O_CLOEXEC);
			if (fd == -1) {
				fprintf(stderr, "open %s: %d\n", copy, error);
				goto out;
			}

			close(fd);
		}
		*delim = '/';
		tmp = delim + 1;
	}

end_of_path:
/* 	error = stat(checkpoint_dir, &statbuf); */
/* 	if (error) { */
/* 		fprintf(stderr, "stat %s: %d\n", checkpoint_dir, error); */
/* 		goto out; */
/* 	} */

	error = chown(checkpoint_dir, statbuf.st_uid, statbuf.st_gid);
	if (error) {
		fprintf(stderr, "chmod %s: %d\n", checkpoint_dir, error);
		goto out;
	}

	fd = open(checkpoint_dir, O_RDONLY|O_NONBLOCK|O_DIRECTORY|O_CLOEXEC);
	if (fd == -1) {
		fprintf(stderr, "open %s: %d\n", checkpoint_dir, error);
		goto out;
	}

	close(fd);
out:
	free(copy);
	return;
}

void parse_args(int argc, char *argv[])
{
	char c;
	int option_index = 0;
	char * short_options= "hqacd:bfu::k::i";
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
		      const char *storage_dir,
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
	snprintf(path, PATH_MAX, "%s/description.txt", storage_dir);

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
			  const char *storage_dir)
{
	/* we cannot clean the checkpoint dir easily since it has been
	 *  given by the user.
	 * There may be some files unrelated with current checkpoint.
	 */

	fprintf(stderr,
		"WARNING: checkpoint has failed. Some files may be left"
		"in the directory %s.\n", storage_dir);
}

int checkpoint_app(long pid, int flags, const char *storage_dir,
		   short _quiet)
{
	int r;

	struct checkpoint_info info;
	if (from_appid) {
		if (!_quiet)
			printf("Checkpointing application %ld...\n", pid);

		info = application_checkpoint_from_appid(pid, flags,
							 storage_dir);
	} else {
		if (!_quiet)
			printf("Checkpointing application in which "
			       "process %d is involved...\n", (pid_t)pid);
		info = application_checkpoint_from_pid((pid_t)pid, flags,
						       storage_dir);
	}

	r = info.result;

	if (!r)
		write_description(description, &info, storage_dir, _quiet);
	else {
		show_error(errno);
		clean_checkpoint_dir(&info, storage_dir);
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

int freeze_checkpoint_unfreeze(long pid, int flags, const char *storage_dir,
			       int signal, short _quiet)
{
	int r;

	r = freeze_app(pid, _quiet);
	if (r)
		goto err_freeze;
	r = checkpoint_app(pid, flags, storage_dir, _quiet);
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
	char *storage_dir = NULL;

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

		lookup_parent_directory(argv[optind+1]);

/* 		storage_dir = canonicalize_file_name(argv[optind+1]); */
/* 		if (!storage_dir) { */
/* 			r = errno; */
/* 			fprintf(stderr, "canonicalize: %s: %s\n", */
/* 				argv[optind+1], strerror(errno)); */
/* 			goto exit; */
/* 		} */

		storage_dir = strdup(argv[optind+1]);

		lookup_parent_directory(storage_dir);

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
		r = checkpoint_app(pid, flags, storage_dir, quiet);
		break;
	case FREEZE:
		r = freeze_app(pid, quiet);
		break;
	case UNFREEZE:
		r = unfreeze_app(pid, sig, quiet);
		break;
	case ALL:
		r = freeze_checkpoint_unfreeze(pid, flags, storage_dir,
					       sig, quiet);
		break;
	}

exit:
	if (storage_dir)
		free(storage_dir);

	if (r)
		exit(EXIT_FAILURE);

	exit(EXIT_SUCCESS);
}
