#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <kerrighed.h>
#include <libkrgcheckpoint.h>
#include "libbi.h"

int numloops = -1 ;
int quiet = 0;
int close_stdbuffers = 0;

const int MAX_BUFFER_SIZE = 4000000;

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

void loop(int quiet, int numloops)
{
	int i = 0, j = 0;

	for (j = 0; j < numloops; j++) {
		printf("cr_disable()...\n");
		cr_disable();
		i = 0;
		__do_all_loops(quiet, &i, 10);
		cr_enable();
		printf("cr_enable()...\n");
		i = 0;
		__do_all_loops(quiet, &i, 10);
	}
}

void print_buffer(char *buffer)
{
	int i;

	for (i = 0; i < MAX_BUFFER_SIZE; i++) {
		printf("%c", buffer[i]);
		if (i % 50 == 0)
			printf("\n");
	}

	printf("\n\n");
}

int restart_cb(void *arg)
{
	char *buffer = arg;

	print_buffer(buffer);

	return 0;
}

int main(int argc, char *argv[])
{
	char buffer[MAX_BUFFER_SIZE];
	int i = 0, res;
	pid_t pid;

	parse_args(argc, argv);

	close_stdioe(close_stdbuffers);

	close_sync_pipe();

	for (i = 0; i < MAX_BUFFER_SIZE; i++)
		buffer[i]='a';

	buffer[MAX_BUFFER_SIZE-1] = '\0';

	print_buffer(buffer);

/* 	res = cr_exclude_on(buffer, MAX_BUFFER_SIZE, NULL, NULL); */
/* 	if (res) */
/* 		fprintf(stderr, "%s:%d - %d - %s\n", */
/* 			__PRETTY_FUNCTION__, __LINE__, */
/* 			getpid(), strerror(errno)); */

/* 	res = cr_exclude_off(buffer); */
/* 	if (res) */
/* 		fprintf(stderr, "%s:%d - %d - %s\n", */
/* 			__PRETTY_FUNCTION__, __LINE__, */
/* 			getpid(), strerror(errno)); */

	res = cr_exclude_on(buffer, MAX_BUFFER_SIZE, restart_cb, buffer);
	if (res)
		fprintf(stderr, "%s:%d - %d - %s\n",
			__PRETTY_FUNCTION__, __LINE__,
			getpid(), strerror(errno));

	pid = fork();

	res = cr_exclude_on(buffer, MAX_BUFFER_SIZE, NULL, NULL);
	if (res)
		fprintf(stderr, "%s:%d - %d - %s\n",
			__PRETTY_FUNCTION__, __LINE__,
			getpid(), strerror(errno));

	loop(quiet, numloops);
	if (pid > 0) {
		int status;
		wait(&status);
		loop(quiet, numloops);
	}

	return 0;
}
