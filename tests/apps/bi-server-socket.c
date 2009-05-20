#include <stdio.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <libkrgcb.h>
#include "libbi.h"

int numloops = -1 ;
int quiet = 0;
int close_stdbuffers = 0;
int signal = 0;
int sockfd;

#define DEFAULT_PORT 4242

void parse_args(int argc, char *argv[])
{
	int c;

	while (1){
		c = getopt(argc, argv, "l:qhcs");
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
		case 's':
			signal = 1;
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
			printf(" -s   : use signal-context C/R callbacks "
			       "instead of thread-context callbacks");
			exit(1);
		}
	}
}

int open_server_socket(void * param)
{
	struct sockaddr_in server_socket;
	int r = 0;

	sockfd = socket(PF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) {
		perror("socket");
		goto err;
	}

	memset(&server_socket, 0, sizeof(server_socket));
	server_socket.sin_family = AF_INET;
	server_socket.sin_addr.s_addr = INADDR_ANY;
	server_socket.sin_port = DEFAULT_PORT;

	r = bind(sockfd, (struct sockaddr *) &server_socket,
		 sizeof(server_socket));
	if (r == -1) {
		perror("bind");
		goto err;
	}

	r = listen(sockfd, 1);
	if (r == -1) {
		perror("listen");
		goto err;
	}

err:
	return r;
}

int close_server_socket(void * param)
{
	int r;
	r = close(sockfd);
	if (r)
		perror("close");

	return r;
}

int main(int argc, char *argv[])
{
	int r;

	parse_args(argc, argv);

	close_stdioe(close_stdbuffers);

	r = open_server_socket(NULL);
	if (r)
		exit(EXIT_FAILURE);

	r = cr_callback_init();
	if (r) {
		fprintf(stderr, "Fail to initialize callback library\n");
		exit(EXIT_FAILURE);
	}

	if (signal)
		r = cr_register_chkpt_callback(&close_server_socket, NULL);
	else
		r = cr_register_chkpt_thread_callback(&close_server_socket,
						      NULL);

	if (r) {
		fprintf(stderr, "Fail to register checkpoint callback\n");
                exit(EXIT_FAILURE);
	}

	if (signal)
		r = cr_register_continue_callback(&open_server_socket, NULL);
	else
		r = cr_register_continue_thread_callback(&open_server_socket,
							 NULL);
	if (r) {
		fprintf(stderr, "Fail to register continue callback\n");
                exit(EXIT_FAILURE);
	}

	if (signal)
		r = cr_register_restart_callback(&open_server_socket, NULL);
	else
		r = cr_register_restart_thread_callback(&open_server_socket,
							NULL);

	if (r) {
		fprintf(stderr, "Fail to register restart callback\n");
                exit(EXIT_FAILURE);
	}

	close_sync_pipe();

	do_all_loops(quiet, numloops);

	close_server_socket(NULL);

	return 0;
}
