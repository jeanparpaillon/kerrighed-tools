#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "libbi.h"

#define KTP_TEST 1

#define SHM_SIZE 1024

int create = 0;
int quiet = 0;
int nb_loops = 0;
char msg[SHM_SIZE];

/* return -1 in case of error */
int create_shm(const char *path)
{
	int error;
	int shmid = -1;

	shmid = shm_open(path, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
	if (shmid == -1)
		fprintf(stderr, "create_shm(%s)::shm_open: %s\n", path,
			strerror(errno));

	error = ftruncate(shmid, SHM_SIZE);
	if (error) {
		fprintf(stderr, "create_shm(%s)::ftruncate: %s\n", path,
			strerror(errno));
		shm_unlink(path);
		return error;
	}

	return shmid;
}

/* return -1 in case of error */
int get_shm(const char *path)
{
	int shmid = -1;

	shmid = shm_open(path, O_RDWR, 0);
	if (shmid == -1)
		fprintf(stderr, "get_shm(%s)::shm_open: %s\n", path,
			strerror(errno));

	return shmid;
}

int delete_shm(const char *path)
{
	int r;

	r = shm_unlink(path);
	if (r)
		fprintf(stderr, "delete_shm(%s)::shm_unlink: %s\n", path,
			strerror(errno));

	return r;
}

void print_usage(const char* cmd)
{
	printf("%s -h: show this help\n", cmd);

	printf("%s -c\"message\" <shm name>:"
	       " initialiaze a shm\n", cmd);

	printf("%s -d <shm name>:"
	       " delete a shm\n", cmd);

	printf("%s -r\"N\" <shm name>:"
	       " read N times from a shm with a small pause between each "
	       "reading\n", cmd);

	printf("%s -w\"message\" <shm name>:"
	       " write to a shm\n", cmd);
}

void parse_args(int argc, char *argv[])
{
	int c;

	while (1) {

		c = getopt(argc, argv, "c:w:r:dqh");
		if (c == -1)
			break;

		switch (c) {
		case 'r':
			nb_loops = atoi(optarg);
			if (nb_loops <= 0) {
				fprintf(stderr, "Invalid value for -r options\n");
				exit(EXIT_FAILURE);
			}
			break;
		case 'c':
			create = 1;
			strncpy(msg, optarg, SHM_SIZE);
			break;
		case 'w':
			strncpy(msg, optarg, SHM_SIZE);
			break;
		case 'd':
			create = -1;
			break;
		case 'q':
			quiet = 1;
			break;
		default:
			fprintf(stderr, "** unknown option\n");
		case 'h':
			print_usage(argv[0]);
			exit(EXIT_FAILURE);
		}
	}
}

void print_msg(const char *format, ...)
{
	if (quiet)
		return;

	va_list params;
	va_start(params, format);
	vprintf(format, params);
	va_end(params);
}


int main(int argc, char* argv[])
{
	int shmid;
	char *data;

	if (argc < 3) {
		print_usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	parse_args(argc, argv);

	if (create == 1) {
		shmid = create_shm(argv[argc-1]);
	} else if (create == -1) {
		/* user wants to remove the SHM object */
		int r;
		r = delete_shm(argv[argc-1]);
		if (r)
			exit(EXIT_FAILURE);

		exit(EXIT_SUCCESS);
	} else {
		shmid = get_shm(argv[argc-1]);
	}

	if (shmid == -1)
		exit(EXIT_FAILURE);

	data = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shmid, 0);
	if (data == MAP_FAILED) {
		perror("mmap");
		exit(EXIT_FAILURE);
	}

	if (nb_loops == 0) {
		strncpy(data, msg, SHM_SIZE);
		print_msg("%d:%s\n", shmid, msg);
	} else {
		int i;

		for (i = 0; i < nb_loops; i++) {
			int n = 0;

			print_msg("%s\n", data);
			do_one_loop(i, &n);
#ifdef KTP_TEST
			if (strncmp(data, "KTP REQ CHANGE", 15) == 0)
				strncpy(data, "KTP CHANGE DONE", SHM_SIZE);
#endif
		}
	}

	if (munmap(data, SHM_SIZE) == -1) {
		perror("munmap");
		exit(EXIT_FAILURE);
	}

	close(shmid);

	exit(EXIT_SUCCESS);
}
