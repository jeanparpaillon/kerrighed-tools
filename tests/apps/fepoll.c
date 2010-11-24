#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>

#include "utils.h"

int test_epoll(int efd, int fd)
{
	int ret;
	struct epoll_event event;

	event.events = EPOLLIN;
	event.data.fd = fd;

	ret = epoll_ctl(efd, EPOLL_CTL_ADD, fd, &event);
	if (ret) {
		perror("epoll_ctl");
		goto exit;
	}

	for (;;) {
		ret = epoll_wait(efd, &event, 1, -1);
		if (ret == -1) {
			if (errno != EINTR) {
				perror("epoll_wait");
				goto exit;
			} else {
				printf("epoll was interrupted!\n");
			}
		} else if (ret == 1) {
			ret = 0;
			break;
		}
	}

	if (fd != event.data.fd) {
		fprintf(stderr, "Wrong fd !?! (%d != %d)\n",
			fd, event.data.fd);
		goto exit;
	}

exit:
	return ret;
}

int main(int argc, char *argv[])
{
	int ret;
	int efd;
	pid_t pid;

	if (argc != 3) {
		printf("usage: %s host port\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	/* fd = open(argv[1], O_RDWR); */
	/* if (fd == -1) { */
	/* 	perror("open"); */
	/* 	exit(EXIT_FAILURE); */
	/* } */

	efd = epoll_create(1);
	if (efd == -1) {
		perror("epoll_create");
		exit(EXIT_FAILURE);
	}

	pid = fork();
	if (pid == -1) {
		perror("fork");
		ret = -1;
		goto exit;
	}

	if (pid == 0) {
		/* child */

		struct addrinfo hints;
		struct addrinfo *result, *rp;
		int sfd, s;

		/* Obtain address(es) matching host/port */
		memset(&hints, 0, sizeof(struct addrinfo));
		hints.ai_family = AF_INET;       /* Allow IPv4 */
		hints.ai_socktype = SOCK_STREAM; /* TCP */
		hints.ai_flags = 0;
		hints.ai_protocol = 0;          /* Any protocol */

		s = getaddrinfo(argv[1], argv[2], &hints, &result);
		if (s != 0) {
			fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
			exit(EXIT_FAILURE);
		}

		for (rp = result; rp != NULL; rp = rp->ai_next) {
			sfd = socket(rp->ai_family, rp->ai_socktype,
				     rp->ai_protocol);
			if (sfd == -1)
				continue;

			if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
				break;                  /* Success */

			close(sfd);
		}

		if (rp == NULL) {               /* No address succeeded */
			fprintf(stderr, "Could not connect\n");
			exit(EXIT_FAILURE);
		}

		freeaddrinfo(result);           /* No longer needed */

		ret = test_epoll(efd, sfd);

		close(sfd);
	} else {
		/* parent */
		ret = check_end_child_status(pid);
		if (ret)
			goto exit;
	}

exit:
	close_and_exit(efd, ret);

	/* never reached */
	return 0;
}

