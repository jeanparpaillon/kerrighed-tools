#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

#include "utils.h"

int testfchmod(int fd, mode_t mode)
{
	int ret = fchmod(fd, mode);
	if (ret)
		perror("fchmod");
	return ret;
}

int check_changes(int fd, struct stat *prev_stat, mode_t mode)
{
	struct stat new_stat;
	int ret;

	ret = fstat(fd, &new_stat);
	if (ret) {
		perror("fstat");
		return ret;
	}

	if (prev_stat->st_mode == new_stat.st_mode) {
		fprintf(stderr,
			"mode has not been applied. "
			"Previous mode was %d\n", prev_stat->st_mode);
		return -1;
	}

	return 0;
}

int main(int argc, char *argv[])
{
	int ret;
	int fd, mode;
	pid_t pid;
	struct stat prev_stat;

	if (argc != 3) {
		printf("usage: %s path <mode>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	fd = open_and_stat(argv[1], &prev_stat);
	if (fd == -1)
		exit(EXIT_FAILURE);

	mode = atoi(argv[2]);

	pid = fork();
	if (pid == -1) {
		perror("fork");
		ret = -1;
		goto exit;
	}

	if (pid == 0) {
		/* child */
		ret = testfchmod(fd, mode);
	} else {
		/* parent */
		ret = check_end_child_status(pid);
		if (ret)
			goto exit;

		ret = check_changes(fd, &prev_stat, mode);
		if (ret)
			goto exit;
	}

exit:
	close_and_exit(fd, ret);

	/* never reached */
	return 0;
}

