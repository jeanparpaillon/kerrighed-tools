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
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sched.h>
#include <string.h>
#include <sys/wait.h>

void print_usage(char *cmd)
{
	fprintf(stderr, "usage: %s [capabilities|--] file program [arg ...]\n", cmd);

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

int main(int argc, char *argv[])
{
	pid_t pid, sid;
	FILE* f;

	if (argc < 4) {
		print_usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	/*
	 * set the right Kerrighed capabilities
	 */
	if (strcmp(argv[1], "--") != 0) {
		char capabilities[256];

		/* quick and dirty... */
		pid = getpid();

		/* remove pre-existing capabilies */
		/* set_capabilities(pid, "-d CHANGE_KERRIGHED_CAP");*/
		/* set_capabilities(pid, "-e CHANGE_KERRIGHED_CAP");*/

		/* setting the new ones */
		snprintf(capabilities, 256, "-d +%s", argv[1]);
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
		exit(EXIT_SUCCESS);
	}

	sid = setsid();

	if (sid < 0) {
		perror("setsid"); /* cannot happen */
		exit(EXIT_FAILURE);
	}

	/* write the session id in the file */
	f = fopen(argv[2], "w");
	if (!f) {
		perror("fopen");
		exit(EXIT_FAILURE);
	}
	fprintf(f, "%d", sid);
	fclose(f);

	execvp(argv[3], argv + 3);
	perror("execvp");
	exit(EXIT_FAILURE);
}
