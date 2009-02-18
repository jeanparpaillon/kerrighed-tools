#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <unistd.h>
#include "libbi.h"

#define KTP_TEST 1

#define SHM_SIZE 1024

int create = 0;
int use_id = 0;
int quiet = 0;
int nb_loops = 0;
char msg[SHM_SIZE];

key_t get_key(const char* path)
{
	key_t key;
	key = ftok(path, 'R');
	if (key == -1)
		perror("ftok");

	return key;
}

/* return -1 in case of error */
int create_shm(const char* path)
{
	key_t key;
	int shmid = -1;

	key = get_key(path);
	if (key == -1)
		return -1;

	shmid = shmget(key, SHM_SIZE, 0644 | IPC_CREAT | IPC_EXCL);
	if (shmid == -1)
		fprintf(stderr, "create_shm(%s)::shmget: %s\n", path,
			strerror(errno));

	return shmid;
}

/* return -1 in case of error */
int get_shm(const char* path)
{
	key_t key;
	int shmid = -1;

	key = get_key(path);
	if (key == -1)
		return -1;

	shmid = shmget(key, 0, 0);
	if (shmid == -1)
		fprintf(stderr, "get_shm(%s)::shmget: %s\n", path,
			strerror(errno));

	return shmid;
}

int delete_shm(int shmid)
{
	int r;
	if (shmid == -1) {
		fprintf(stderr, "delete_shm(%d)::shmctl: invalid id: %s\n", shmid,
			strerror(-EINVAL));
		return -EINVAL;
	}

	r = shmctl(shmid, IPC_RMID, NULL);
	if (r)
		fprintf(stderr, "delete_shm(%d)::shmctl: %s\n", shmid,
			strerror(errno));

	return r;
}

void print_usage(const char* cmd)
{
	printf("%s -h: show this help\n", cmd);

	printf("%s -c\"message\" /path/to/shm:"
	       " initialiaze a shm\n", cmd);

	printf("%s -d {/path/to/shm | -i <shmid>}:"
	       " delete a shm\n", cmd);

	printf("%s -r\"N\" {/path/to/shm | -i <shmid>}:"
	       " read N times from a shm with a small pause between each "
	       "reading\n", cmd);

	printf("%s -w\"message\" {/path/to/shm | -i <shmid>}:"
	       " write to a shm\n", cmd);
}

void parse_args(int argc, char *argv[])
{
	int c;

	while (1) {

		c = getopt(argc, argv, "c:w:r:diqh");
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
		case 'i':
			use_id = 1;
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

	if (use_id && create == 1) {
		fprintf(stderr, "** incompatible options used: -c and -i\n");
		exit(EXIT_FAILURE);
	}

	if (create == 1) {
		shmid = create_shm(argv[argc-1]);
	} else {
		if (use_id) {
			shmid = atoi(argv[argc-1]);
		} else {
			shmid = get_shm(argv[argc-1]);
		}

		if (create == -1) { /* user wants to remove the SHM object */
			int r;
			r = delete_shm(shmid);
			if (r)
				exit(EXIT_FAILURE);

			exit(EXIT_SUCCESS);
		}
	}

	if (shmid == -1)
		exit(EXIT_FAILURE);

	data = shmat(shmid, (void *)0, 0);
	if (data == (char *)(-1)) {
		perror("shmat");
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

	if (shmdt(data) == -1) {
		perror("shmdt");
		exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);
}
