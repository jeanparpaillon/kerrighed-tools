#define _BSD_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include "utils.h"

int testfutimes(int fd, const struct timeval tv[2])
{
	int ret;

	ret = futimes(fd, tv);
	if (ret) {
		perror("futimes");
		goto out;
	}

out:
	return ret;
}

int check_changes(int fd, struct stat *prev_stat)
{
	struct stat new_stat;
	int ret;

	ret = fstat(fd, &new_stat);
	if (ret) {
		perror("fstat");
		return ret;
	}

	if (prev_stat->st_mtime == new_stat.st_mtime) {
		fprintf(stderr,
			"mtime has not been changed. "
			"mtime is %ld\n", prev_stat->st_mtime);
		return -1;
	}

	return 0;
}

int main(int argc, char *argv[])
{
	int ret;
	int fd;
	pid_t pid;
	struct stat prev_stat;

	if (argc != 2) {
		printf("usage: %s path\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	fd = open_and_stat(argv[1], &prev_stat);
	if (fd == -1)
		exit(EXIT_FAILURE);

	pid = fork();
	if (pid == -1) {
		perror("fork");
		ret = -1;
		goto exit;
	}

	if (pid == 0) {
		/* child */
		ret = testfutimes(fd, NULL);
	} else {
		/* parent */
		ret = check_end_child_status(pid);
		if (ret)
			goto exit;

		ret = check_changes(fd, &prev_stat);
		if (ret)
			goto exit;
	}

exit:
	close_and_exit(fd, ret);

	/* never reached */
	return 0;
}

