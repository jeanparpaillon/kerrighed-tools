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

void print_buffer(char *buffer)
{
	int i;

	for (i = 0; i < MAX_BUFFER_SIZE; i++) {
		printf("%x", buffer[i]);
		if (i % 20 == 0)
			printf("\n");
	}

	printf("\n\n");
}

int restart_cb(void *arg)
{
	char *buffer = arg;
	int garbage = 0;
	int i;

	for (i = 0; i < MAX_BUFFER_SIZE; i++) {
		if (buffer[i] != 'a')
			garbage = 1;
		buffer[i]='b';
	}

	if (!garbage) {
		fprintf(stderr,
			"Error: memory data region has not "
			"been replaced by garbage\n");
		exit(EXIT_FAILURE);
	}

	if (!quiet)
		print_buffer(buffer);

	return 0;
}

int main(int argc, char *argv[])
{
	char buffer[MAX_BUFFER_SIZE];
	int i = 0, res;

	parse_args(argc, argv);

	close_stdioe(close_stdbuffers);

	for (i = 0; i < MAX_BUFFER_SIZE; i++)
		buffer[i]='a';

	buffer[MAX_BUFFER_SIZE-1] = '\0';

	if (!quiet)
		print_buffer(buffer);

	res = cr_exclude_on(buffer, MAX_BUFFER_SIZE, restart_cb, buffer);
	if (res)
		fprintf(stderr, "%s:%d - %d - %s\n",
			__PRETTY_FUNCTION__, __LINE__,
			getpid(), strerror(errno));

	fork();

	close_sync_pipe();

	do_all_loops(quiet, numloops);

	res = cr_exclude_off(buffer);

	return 0;
}
