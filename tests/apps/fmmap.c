#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>

#include "utils.h"

int test_fmmap(int fd)
{
	void *addr = mmap(NULL, 32, PROT_READ, MAP_SHARED,
			  fd, 0);

	if (addr == MAP_FAILED) {
		perror("mmap");
		return -1;
	}

	printf("%s\n", (char*)addr);

	munmap(addr, 32);

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
		ret = test_fmmap(fd);
	} else {
		/* parent */
		ret = check_end_child_status(pid);
		if (ret)
			goto exit;
	}

exit:
	close_and_exit(fd, ret);

	/* never reached */
	return 0;
}

