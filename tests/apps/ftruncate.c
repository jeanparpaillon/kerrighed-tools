#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

#include "utils.h"

int testftruncate(int fd, off_t length)
{
	int ret = ftruncate(fd, length);
	if (ret)
		perror("ftruncate");
	return ret;
}

int check_changes(int fd, struct stat *prev_stat, off_t length)
{
	struct stat new_stat;
	int ret;

	ret = fstat(fd, &new_stat);
	if (ret) {
		perror("fstat");
		return ret;
	}

	if (prev_stat->st_size == new_stat.st_size) {
		fprintf(stderr,
			"size has not changed. Size is %ld\n",
			prev_stat->st_size);
		return -1;
	}

	if (length != new_stat.st_size) {
		fprintf(stderr,
			"size has changed bug wrongly! (%ld != %ld)\n",
			length, new_stat.st_size);
		return -1;
	}

	return 0;
}

int main(int argc, char *argv[])
{
	int ret;
	int fd;
	off_t length;
	pid_t pid;
	struct stat prev_stat;

	if (argc != 3) {
		printf("usage: %s path <length>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	fd = open_and_stat(argv[1], &prev_stat);
	if (fd == -1)
		exit(EXIT_FAILURE);

	length = atoi(argv[2]);

	pid = fork();
	if (pid == -1) {
		perror("fork");
		ret = -1;
		goto exit;
	}

	if (pid == 0) {
		/* child */
		ret = testftruncate(fd, length);
	} else {
		/* parent */
		ret = check_end_child_status(pid);
		if (ret)
			goto exit;

		ret = check_changes(fd, &prev_stat, length);
		if (ret)
			goto exit;
	}

exit:
	close_and_exit(fd, ret);

	/* never reached */
	return 0;
}

