/*
 * setsid.c -- execute a command in a new session
 * Rick Sladkey <jrs@world.std.com>
 * In the public domain.
 *
 * 1999-02-22 Arkadiusz Mi¶kiewicz <misiek@pld.ORG.PL>
 * - added Native Language Support
 *
 * 2001-01-18 John Fremlin <vii@penguinpowered.com>
 * - fork in case we are process group leader
 *
 * 2008-04-11 Matthieu Fertré <mfertre@irisa.fr>
 * - write the new session id in the file argv[1]
 *
 * 2008-10-17 Matthieu Fertré <mfertre@irisa.fr>
 * - use vfork to ensure that child process has made a call to execve
 * - allow to give Kerrighed capabilities to child process
 *
 * 2009-02-04 Matthieu Fertré <matthieu.fertre@kerlabs.com>
 * - use getopt to manage options
 *
 * 2009-02-11 Matthieu Fertré <matthieu.fertre@kerlabs.com>
 * - add a signal handler on sigterm/sigint to kill the children
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sched.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <getopt.h>
#include <kerrighed.h>

#define MAX_FILEPATH 256
#define MAX_LEN_ARGS 512

short background = 0;
short sync_w_pipe = 0;
short setcap = 1;
short quiet = 0;
char filepath[MAX_FILEPATH];
pid_t pid;

void print_usage(char *cmd)
{
	fprintf(stderr,
		"usage: %s [OPTIONS] -- program [arg ...]\n"
		"\t -o file: write the session id in a file\n"
		"\t -b: start the program in background (default: foreground)\n"
		"\t -q: (quiet) do not show the Application idendifier\n"
		, cmd);

	/* Option -s and -n are hidden, there exist only for tests */
}

void give_checkpointable_capability()
{
	krg_cap_t caps;
	int r;

	r = krg_capget(&caps);
	if (r) {
		fprintf(stderr,
			"Fail to get Kerrighed capabilities "
			"of current process (%d)\n", getpid());
		exit(EXIT_FAILURE);
	}

	caps.krg_cap_inheritable_effective |= (1 << CAP_CHECKPOINTABLE);

	r = krg_capset(&caps);
	if (r) {
		fprintf(stderr, "Fail to set Kerrighed capabilities "
			"of current process (%d)\n", getpid());
		exit(EXIT_FAILURE);
	}
}

void parse_args(int argc, char *argv[])
{
	int c;

	while (1) {
		c = getopt(argc, argv, "hbo:qsn");
		if (c == -1)
			break;
		switch (c) {
		case 'b':
			background = 1;
			break;
		case 'o':
			snprintf(filepath, MAX_FILEPATH, "%s", optarg);
			break;
		case 'h':
			print_usage(argv[0]);
			exit(EXIT_SUCCESS);
			break;
		case 'q':
			quiet = 1;
			break;
		case 's':
			sync_w_pipe = 1;
			break;
		case 'n':
			setcap = 0;
			break;
		default:
			printf("** unknown option\n");
			print_usage(argv[0]);
			exit(EXIT_FAILURE);
		}
	}

	if (!background)
		sync_w_pipe = 0;
}

void relay_signal(int signum)
{
	kill(pid, signum);
}

int main(int argc, char *argv[])
{
	pid_t sid;
	FILE* f;
	int pipefd[2];

	if (argc < 2) {
		print_usage(argv[0]);
		exit(EXIT_FAILURE);
	}
	filepath[0]='\0';

	parse_args(argc, argv);

	/*
	 * set the Kerrighed CHECKPOINT capability
	 */
	if (setcap)
		give_checkpointable_capability();

	/* open a pipe to synchronize with the child application */
	if (sync_w_pipe &&
	    pipe(pipefd) == -1) {
		perror("pipe");
		exit(EXIT_FAILURE);
	}

	/*
	 * vfork ensures that the child process has "exec"ed when
	 * the parent will exit
	 */
	pid = vfork();

	switch(pid) {
	case -1:
		perror("vfork");
		exit(EXIT_FAILURE);
	case 0:		/* child */
		break;
	default:	/* parent */
		if (!quiet)
			printf("Running application %d\n", pid);

		if (!background) {
			int r, exit_status;
			struct sigaction action;

			action.sa_handler = &relay_signal;
			r = sigaction(SIGINT, &action, NULL);
			if (r)
				perror("sigaction");
			r = sigaction(SIGTERM, &action, NULL);
			if (r)
				perror("sigaction");

			wait(&exit_status);
			if (exit_status)
				exit(EXIT_FAILURE);
		} else if (sync_w_pipe) {
			int r;
			char buff[2];

			close(pipefd[1]);

			r = read(pipefd[0], buff, 1);
			if (r == -1)
				perror("read");
			close(pipefd[0]);

		}

		exit(EXIT_SUCCESS);
	}

	if (sync_w_pipe)
		close(pipefd[0]);

	sid = setsid();

	if (sid < 0) {
		perror("setsid"); /* cannot happen */
		exit(EXIT_FAILURE);
	}

	/* write the session id in the file */
	if (strcmp(filepath, "") != 0) {
		f = fopen(filepath, "w");
		if (!f) {
			perror("fopen");
			exit(EXIT_FAILURE);
		}
		fprintf(f, "%d", sid);
		fclose(f);
	}

	execvp(argv[optind], argv + optind);
	perror("execvp");
	exit(EXIT_FAILURE);
}
