#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sched.h>
#include <syscall.h>
#include <pthread.h>
#include "libbi.h"

#define NB_THREADS 3

int i=0;
int j=0;
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


void * main_loop()
{
	__do_all_loops(quiet, &j, numloops);
	return NULL;
}

int main(int argc, char *argv[])
{
	int i;
	int r[NB_THREADS];
	pthread_t thread[NB_THREADS];

	parse_args(argc, argv);

	close_stdioe(close_stdbuffers);

	for (i=0; i<NB_THREADS; i++)
		r[i] = pthread_create(&(thread[i]), NULL, main_loop, NULL);

	close_sync_pipe();

	for (i=0; i<NB_THREADS; i++)
		pthread_join(thread[i], NULL);

	return 0;
}
