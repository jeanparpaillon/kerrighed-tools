#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <signal.h>
#include "libbi.h"

int numloops = -1 ;
int quiet = 0;
int close_stdbuffers = 0;

void parse_args(int argc, char *argv[])
{
	int c;

	while (1){
		c = getopt(argc, argv, "l:qhc");
		if (c == -1)
			break;
		switch (c) {
		case 'l':
			numloops = atoi(optarg);
			break;
		case 'q':
			quiet = 1;
			break;
		case 'c':
			quiet = 1;
			close_stdbuffers = 1;
			break;
		default:
			printf("** unknown option\n");
		case 'h':
			printf("usage: %s [-h] [-l N] [-q] [-c]\n", argv[0]);
			printf(" -h   : this help\n");
			printf(" -l N : number of loops\n");
			printf(" -q   : quiet\n");
			printf(" -c   : quiet + close the stdin, stdout, "
			       "and stderr\n");
			exit(1);
		}
	}
}

void signal_handler(int signum)
{
	if (!quiet)
		printf("receive signal %d\n", signum);
}


int main(int argc, char *argv[])
{
	int r;
	struct sigaction action;

	parse_args(argc, argv);

	close_stdioe(close_stdbuffers);

	action.sa_handler = signal_handler;
	sigemptyset (&action.sa_mask);
	action.sa_flags = 0;
	r = sigaction(SIGUSR1, &action, NULL);
	if (r) {
		perror("sigaction");
		return r;
	}

	close_sync_pipe();

	do_all_loops(quiet, numloops);

	return 0;
}
