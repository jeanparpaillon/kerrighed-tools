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
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sched.h>
#include <string.h>
#include <sys/wait.h>
#include <getopt.h>

#define MAX_LEN_CAP 256
#define MAX_FILEPATH 256

short foreground = 0;
char inheritable_capabilities[MAX_LEN_CAP];
char filepath[MAX_FILEPATH];


void print_usage(char *cmd)
{
	fprintf(stderr,
		"usage: %s [-c capabilities] [-o file] [-f] "
		"program [arg ...]\n", cmd);

}

void set_capabilities(pid_t pid, char* args)
{
	int r;
	char krgcapsetcmd[256];

	snprintf(krgcapsetcmd, 256, "krgcapset -k %d %s", pid, args);

	r = system(krgcapsetcmd);

	if (r) {
		int r_kcap;
		perror("system");
		r_kcap = WEXITSTATUS(r);
		fprintf(stderr, "%s exits with status %d\n",
			krgcapsetcmd, r_kcap);
		exit(EXIT_FAILURE);
	}
}

void parse_args(int argc, char *argv[])
{
	int c;

	while (1){
		c = getopt(argc, argv, "hfo:c:");
		if (c == -1)
			break;
		switch (c) {
		case 'c':
			snprintf(inheritable_capabilities, MAX_LEN_CAP, optarg);
			break;
		case 'f':
			foreground = 1;
			break;
		case 'o':
			snprintf(filepath, MAX_FILEPATH, optarg);
			break;
		case 'h':
			print_usage(argv[0]);
			exit(EXIT_SUCCESS);
			break;
		default:
			printf("** unknown option\n");
			print_usage(argv[0]);
			exit(EXIT_FAILURE);
		}
	}
}

int main(int argc, char *argv[])
{
	pid_t pid, sid;
	FILE* f;

	if (argc < 2) {
		print_usage(argv[0]);
		exit(EXIT_FAILURE);
	}
	inheritable_capabilities[0]='\0';
	filepath[0]='\0';

	parse_args(argc, argv);

	/*
	 * set the right Kerrighed capabilities
	 */
	if (strcmp(inheritable_capabilities, "") != 0) {
		char capabilities[MAX_LEN_CAP];

		/* quick and dirty... */
		pid = getpid();

		/* setting the new ones */
		snprintf(capabilities, MAX_LEN_CAP, "-d %s",
			 inheritable_capabilities);
		set_capabilities(pid, capabilities);
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
		if (foreground) {
			int exit_status;
			wait(&exit_status);
			if (exit_status)
				exit(EXIT_FAILURE);
		}

		exit(EXIT_SUCCESS);
	}

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
