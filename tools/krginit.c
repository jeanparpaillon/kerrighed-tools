#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <hotplug.h>

#include <config.h>

int create_session = 0;

void version(char * program_name)
{
	printf("\
%s %s\n\
Copyright (C) 2010 Kerlabs.\n\
This is free software; see source for copying conditions. There is NO\n\
warranty; not even for MERCHANBILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\
\n", program_name, VERSION);
}

void help(char * program_name)
{
	fprintf(stderr,
		"Usage: %s [-s] [-h] [-v] [<init_program> [<init_arg1> ...]]\n"
		"Options:\n"
		" -s  Create a process session\n"
		" -h  Display these lines\n"
		" -v  Display version informations\n",
		program_name);
}

int get_config(int argc, char *argv[])
{
	int opt;

	while ((opt = getopt(argc, argv, "svh")) != -1) {
		switch (opt) {
		case 's':
			create_session = 1;
			break;
		case 'v':
			version(argv[0]);
			exit(EXIT_SUCCESS);
		case 'h':
			help(argv[0]);
			exit(EXIT_SUCCESS);
		default:
			help(argv[0]);
			exit(EXIT_FAILURE);
		}
	}

	return optind;
}

int main(int argc, char *argv[])
{
	int exec_ind;
	int ret;

	exec_ind = get_config(argc, argv);

	if (create_session) {
		ret = setsid();
		if (ret == -1) {
			fprintf(stderr, "%s setsid() failed! %s\n",
				argv[0], strerror(errno));
			exit(1);
		}
	}

	krg_node_ready(1);

	if (exec_ind < argc) {
		ret = execv(argv[exec_ind], argv + exec_ind);
		fprintf(stderr,
			"%s execv(%s, ...) failed! %s\n",
			argv[0], argv[exec_ind], strerror(errno));
		exit(1);
	}

	/* No real init? Behave as a basic child reaper */
	for (;;) {
		ret = wait(NULL);
		if (ret == -1 && errno == ECHILD)
			sleep(1);
	}

	return 0;
}
