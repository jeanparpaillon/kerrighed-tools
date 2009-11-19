#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "libbi.h"

int quiet = 0;
int close_stdbuffers = 0;
int pipefd[2];

void parse_args(int argc, char *argv[])
{
	int c;

	while (1){
		c = getopt(argc, argv, "qhc");
		if (c == -1)
			break;
		switch (c) {
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
			printf("usage: %s [-h] [-q] [-c]\n", argv[0]);
			printf(" -h   : this help\n");
			printf(" -q   : quiet\n");
			printf(" -c   : quiet + close the stdin, stdout, "
			       "and stderr\n");
			exit(1);
		}
	}
}

void forward_urandom_content()
{
	int urandom;
	size_t nbbytes;
	ssize_t bytes_read, bytes_written;
	char buffer[128];

	urandom = open("/dev/urandom", O_RDONLY);
	if (urandom == -1) {
		perror("/dev/urandom");
		exit(EXIT_FAILURE);
	}

	nbbytes = sizeof(buffer);

	while (1) {

		bytes_read = read(urandom, buffer, nbbytes);
		if (bytes_read == -1) {
			perror("read");
			exit(EXIT_FAILURE);
		}

		bytes_written = write(pipefd[1], buffer, nbbytes);
		if (bytes_written == -1) {
			perror("write");
			exit(EXIT_FAILURE);
		}
	}

	close(pipefd[1]);
	close(urandom);
}

void read_from_pipe()
{
	char buffer[128];
	size_t nbbytes;
	char bytes_read;

	nbbytes = sizeof(buffer);

	while (1) {
		bytes_read = read(pipefd[0], buffer, nbbytes);
		if (bytes_read == -1) {
			perror("read");
			exit(EXIT_FAILURE);
		}

		if (!quiet)
			printf("%128s\n", buffer);
	}

	close(pipefd[0]);
}

int main(int argc, char *argv[])
{
	pid_t pid;

	parse_args(argc, argv);

	close_stdioe(close_stdbuffers);

	if (pipe(pipefd) == -1) {
		perror("pipe");
		exit(EXIT_FAILURE);
	}

	pid = fork();

	if (pid == -1) {
		perror("clone");
		exit(EXIT_FAILURE);
	}

	if (pid == 0) {
		close_sync_pipe();

		close(pipefd[0]);

		forward_urandom_content();

	} else {
		close_sync_pipe();

		close(pipefd[1]);

		read_from_pipe();
	}

	return 0;
}
