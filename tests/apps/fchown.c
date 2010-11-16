#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

#include "utils.h"

int testfchown(int fd, uid_t owner, gid_t group)
{
	int ret = fchown(fd, owner, group);
	if (ret)
		perror("fchown");
	return ret;
}

int check_changes(int fd, struct stat *prev_stat, uid_t owner, gid_t group)
{
	struct stat new_stat;
	int ret;

	ret = fstat(fd, &new_stat);
	if (ret) {
		perror("fstat");
		return ret;
	}

	if (prev_stat->st_uid == new_stat.st_uid) {
		fprintf(stderr,
			"uid has not been changed. "
			"Previous uid was %d\n", prev_stat->st_uid);
		return -1;
	}

	if (owner != new_stat.st_uid) {
		fprintf(stderr,
			"uid has changed but wrongly! (%d != %d)\n",
			owner, new_stat.st_uid);
		return -1;
	}

	if (prev_stat->st_gid == new_stat.st_gid) {
		fprintf(stderr,
			"group has not been changed. "
			"Previous group was %d\n", prev_stat->st_gid);
		return -1;
	}

	if (group != new_stat.st_gid) {
		fprintf(stderr,
			"group has changed but wrongly! (%d != %d)\n",
			group, new_stat.st_gid);
		return -1;
	}

	return 0;
}

int main(int argc, char *argv[])
{
	int ret;
	int fd;
	uid_t owner;
	gid_t group;
	pid_t pid;
	struct stat prev_stat;

	if (argc != 4) {
		printf("usage: %s path <uid> <gid>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	fd = open_and_stat(argv[1], &prev_stat);
	if (fd == -1)
		exit(EXIT_FAILURE);

	owner = atoi(argv[2]);
	group = atoi(argv[3]);

	pid = fork();
	if (pid == -1) {
		perror("fork");
		ret = -1;
		goto exit;
	}

	if (pid == 0) {
		/* child */
		ret = testfchown(fd, owner, group);
	} else {
		/* parent */
		ret = check_end_child_status(pid);
		if (ret)
			goto exit;

		ret = check_changes(fd, &prev_stat, owner, group);
		if (ret)
			goto exit;
	}

exit:
	close_and_exit(fd, ret);

	/* never reached */
	return 0;
}

