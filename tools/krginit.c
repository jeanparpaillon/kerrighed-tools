#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <hotplug.h>

int create_session = 0;

int get_config(int argc, char *argv[])
{
	int opt;

	while ((opt = getopt(argc, argv, "s")) != -1) {
		switch (opt) {
		case 's':
			create_session = 1;
			break;
		default:
			fprintf(stderr,
				"Usage: %s [-s] [<init_program> [<init_arg1> ...]]\n"
				"Options:\n"
				" -s  Create a process session\n",
				argv[0]);
			exit(1);
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
