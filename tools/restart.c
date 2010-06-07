/*
 *  Copyright (C) 2001-2006, INRIA, Universite de Rennes 1, EDF.
 *  Copyright (C) 2010 Kerlabs.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
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

#include <config.h>

long appid;
int version;
int flags = 0;
pid_t root_pid = 0;
int options = 0;
#define FOREGROUND	1
#define STDIN_OUT_ERR	2
#define QUIET		4
#define DEBUG		8

struct cr_subst_files_array substitution;

int array_size = 0;
const int ARRAY_SIZE_INC = 32;

void show_version(char * program_name)
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
	printf ("Usage: %s [options] appid version\n"
		"\n"
		"Options:\n"
		"  -h|--help                Display this information and exit\n"
		"  -v|--version             Display version information\n"
		"  -t|--replace-tty         Replace application original terminal by the current one\n"
		"  -f|--foreground          Restart the application in foreground\n"
		"  -p|--pids                Replace application orphan pgrp and sid by the ones of restart\n"
		"  -s|--substitute-file file_id,fd\n"
		"                           Substitute application open files by restart parent fd\n",
		program_name);
}

char *__get_returned_word(const char *toexec)
{
	FILE *pipe;
	char *buff;

	pipe = popen(toexec, "r");
	if (!pipe) {
		perror(toexec);
		return NULL;
	}

	buff = malloc(1024);
	if (!buff) {
		fprintf(stderr,"restart: %s\n", strerror(ENOMEM));
		goto error;
	}
	buff[0] = '\0'; /* in case of failure of fread... */

	fread(buff, 1024, 1, pipe);
	buff = strsep(&buff, " \r\n");

	if (options & DEBUG)
		printf("DEBUG: execution of:\n"
		       "\t\t\"%s\"\n"
		       "\t\treturns %s\n",
		       toexec, buff);
error:
	pclose(pipe);

	return buff;
}

char *get_returned_word(char *cmd, ...)
{
	int r;
	va_list args;
	char *buffer, *result = NULL;

	va_start(args, cmd);
	r = vasprintf(&buffer, cmd, args);
	va_end(args);

	if (r == -1)
		goto error;

	result = __get_returned_word(buffer);

	free(buffer);
error:
	return result;
}

char *get_fd_key(const char *checkpoint_dir, const char *pid, int fd)
{
	return get_returned_word(
		"grep -E '[,|]%s:%d(,|$)' %s/user_info_*.txt | cut -d'|' -f 2",
		pid, fd, checkpoint_dir);
}

char *get_root_pid(const char *checkpoint_dir)
{
	return get_returned_word(
		"grep '^Identifier:' %s/description.txt | tr -d ' \n' | cut -d':' -f 2",
		checkpoint_dir);
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

int replace_fd(const char *checkpoint_dir, const char *root_pid, FILE *file)
{
	int i, r, fd;
	char *fd_key;

	fd = fileno(file);
	if (fd == -1) {
		r = -EBADF;
		goto error;
	}

	fd_key = get_fd_key(checkpoint_dir, root_pid, fd);
	if (!fd_key) {
		r = -ENOENT;
		goto error;
	}

	if (strlen(fd_key) == 0) {
		r = -ENOENT;
		goto err_free_key;
	}
	i = substitution.nr;

	r = inc_substitution_array_size(&substitution, array_size);
	if (r < 0)
		goto err_free_key;

	array_size = r;

	substitution.files[i].file_id = fd_key;
	substitution.files[i].fd = fd;

	if (options & DEBUG)
		printf("DEBUG: substitution of file %s by fd %d\n",
		       fd_key, fd);

	r = 0;
error:
	return r;

err_free_key:
	free(fd_key);
	goto error;
}

int replace_stdin_stdout_stderr(const char *checkpoint_dir)
{
	int r;
	char *root_pid;

	root_pid = get_root_pid(checkpoint_dir);
	if (!root_pid) {
		r = -EINVAL;
		goto out;
	}

	if (options & DEBUG)
		printf("DEBUG: root pid: %s\n",
		       root_pid);

	r = replace_fd(checkpoint_dir, root_pid, stdin);
	if (r)
		goto out;

	r = replace_fd(checkpoint_dir, root_pid, stdout);
	if (r)
		goto out;

	r = replace_fd(checkpoint_dir, root_pid, stderr);
	if (r)
		goto out;

	free(root_pid);
out:
	if (r)
		fprintf(stderr, "restart: unable to substitute "
			"stdin, stdout, stderr: %s\n", strerror(-r));
	return r;
}

int parse_args(int argc, char *argv[])
{
	char *checkpoint_dir;
	char c;
	int r, option_index = 0;
	char * short_options= "hvftps:qd";
	static struct option long_options[] =
		{
			{"help", no_argument, 0, 'h'},
			{"version", no_argument, 0, 'v'},
			{"foreground", no_argument, 0, 'f'},
			{"tty", no_argument, 0, 't'},
			{"pids", no_argument, 0, 'p'},
			{"substitute-file", required_argument, 0, 's'},
			{"quiet", no_argument, 0, 'q'},
			{"debug", no_argument, 0, 'd'},
			{0, 0, 0, 0}
		};

	r = 0;

	while ((c = getopt_long(argc, argv, short_options,
				long_options, &option_index)) != -1) {
		switch (c)
		{
		case 'h':
			show_help(argv[0]);
			exit(EXIT_SUCCESS);
		case 'v':
			show_version(argv[0]);
			exit(EXIT_SUCCESS);
		case 'f':
			options |= (FOREGROUND | STDIN_OUT_ERR);
			break;
		case 't':
			options |= STDIN_OUT_ERR;
			break;
		case 'p':
			flags |= APP_REPLACE_PGRP_SID;
			break;
		case 's':
			r = parse_file_substitution(optarg);
			break;
		case 'q':
			options |= QUIET;
			break;
		case 'd':
			options |= DEBUG;
			break;
		default:
			show_help(argv[0]);
			exit(EXIT_FAILURE);
			break;
		}

		if (r) {
			fprintf(stderr,
				"restart: fail to parse args: %s\n",
				strerror(-r));
			exit(EXIT_FAILURE);
		}
	}

	if (argc - optind != 2) {
		show_help(argv[0]);
		exit(EXIT_FAILURE);
	}

	appid = atol(argv[optind]);
	version = atoi(argv[optind+1]);
	asprintf(&checkpoint_dir, CHKPT_DIR "/%ld/v%d/", appid, version);

	if (options & STDIN_OUT_ERR)
		r = replace_stdin_stdout_stderr(checkpoint_dir);

	free(checkpoint_dir);

	return r;
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

int main(int argc, char *argv[])
{
	int r = 0;

	/* Manage options with getopt */
	r = parse_args(argc, argv);
	if (r)
		goto exit;

	/* Check environment */
	check_environment();

	if (!(options & QUIET))
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
		fprintf(stderr, "restart: error during callback execution"
			" for application %ld\n", appid);
		goto exit;
	}

	r = application_unfreeze_from_appid(appid, 0);
	if (r) {
		fprintf(stderr, "restart: fail to unfreeze application %ld: "
			"%s\n", appid, strerror(errno));
		goto exit;
	}

	if (!(options & QUIET))
		printf("Application %ld has been successfully restarted\n",
		       appid);

	if (options & FOREGROUND)
		wait_application_exits();

exit:
	clean_file_substitution(&substitution);

	if (r)
		exit(EXIT_FAILURE);

	exit(EXIT_SUCCESS);
}
