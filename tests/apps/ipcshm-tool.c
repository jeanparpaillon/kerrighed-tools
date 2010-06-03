#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "libbi.h"

#define KTP_TEST 1

#define DEFAULT_SHM_SIZE 1024

int use_id = 0;
int quiet = 0;
int nb_loops = 0;
size_t shm_size = DEFAULT_SHM_SIZE;

typedef enum {
	NONE,
	CREATE,
	CREATE_FROM_FILE,
	DELETE,
	READ,
	WRITE,
	WRITE_FROM_FILE
} action_t;

action_t action;

char *msg = NULL;
char *path_msg = NULL;

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

	shmid = shmget(key, shm_size, 0644 | IPC_CREAT | IPC_EXCL);
	if (shmid == -1)
		fprintf(stderr, "create_shm(%s)::shmget: %s\n", path,
			strerror(errno));

	return shmid;
}

/* return -1 in case of error */
int get_shm(const char* path)
{
	key_t key;
	struct shmid_ds buf;
	int shmid = -1, r;

	key = get_key(path);
	if (key == -1)
		return -1;

	shmid = shmget(key, 0, 0);
	if (shmid == -1) {
		fprintf(stderr, "get_shm(%s)::shmget: %s\n", path,
			strerror(errno));
		return shmid;
	}

	r = shmctl(shmid, IPC_STAT, &buf);
	if (r) {
		fprintf(stderr, "get_shm(%s)::shmctl: %s\n", path,
			strerror(errno));
		return r;
	}
	shm_size = buf.shm_segsz;

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

	printf("%s -C\"/path/to/file/containing/message\" /path/to/shm:"
	       " initialiaze a shm\n", cmd);

	printf("%s -d {/path/to/shm | -i <shmid>}:"
	       " delete a shm\n", cmd);

	printf("%s -r\"N\" {/path/to/shm | -i <shmid>}:"
	       " read N times from a shm with a small pause between each "
	       "reading\n", cmd);

	printf("%s -w\"message\" {/path/to/shm | -i <shmid>}:"
	       " write to a shm\n", cmd);

	printf("%s -W\"/path/to/file/containing/message\" {/path/to/shm | -i <shmid>}:"
	       " write to a shm\n", cmd);
}

void parse_args(int argc, char *argv[])
{
	int c;

	while (1) {

		c = getopt(argc, argv, "c:C:w:W:r:diqh");
		if (c == -1)
			break;

		switch (c) {
		case 'r':
			action = READ;
			nb_loops = atoi(optarg);
			if (nb_loops <= 0) {
				fprintf(stderr, "Invalid value for -r options\n");
				exit(EXIT_FAILURE);
			}
			break;
		case 'c':
			action = CREATE;
			shm_size = strlen(optarg)+1;
			msg = optarg;
			break;
		case 'C':
			action = CREATE_FROM_FILE;
			path_msg = optarg;
			break;
		case 'w':
			action = WRITE;
			msg = optarg;
			break;
		case 'W':
			action = WRITE_FROM_FILE;
			path_msg = optarg;
			break;
		case 'd':
			action = DELETE;
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
	int shmid, r;
	char *data;

	if (argc < 3) {
		print_usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	parse_args(argc, argv);

	if (action == NONE) {
		print_usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	if (action == CREATE || action == CREATE_FROM_FILE) {

		if (use_id) {
			fprintf(stderr, "** incompatible options used: -c|-C and -i\n");
			exit(EXIT_FAILURE);
		}

		if (action == CREATE)
			shm_size = strlen(msg) + 1;
		else {
			/* CREATE_FROM_FILE */
			struct stat buf;
			r = stat(path_msg, &buf);
			if (r) {
				perror("stat");
				exit(EXIT_FAILURE);
			}
			shm_size = buf.st_size;
			print_msg("%d: size: %zd\n", shmid, shm_size);
		}

		shmid = create_shm(argv[argc-1]);

	} else if (use_id) {
		shmid = atoi(argv[argc-1]);
	} else {
		shmid = get_shm(argv[argc-1]);
	}

	if (shmid == -1)
		exit(EXIT_FAILURE);

	if (action == DELETE) {
		r = delete_shm(shmid);
		if (r)
			exit(EXIT_FAILURE);
		exit(EXIT_SUCCESS);
	}

	data = shmat(shmid, (void *)0, 0);
	if (data == (char *)(-1)) {
		perror("shmat");
		exit(EXIT_FAILURE);
	}

	if (action == CREATE || action == WRITE) {
		memcpy(data, msg, shm_size);
		print_msg("%d:%s\n", shmid, msg);
	} else if (action == CREATE_FROM_FILE || action == WRITE_FROM_FILE) {
		r = open(path_msg, O_RDONLY);
		if (r == -1) {
			perror("open");
			exit(EXIT_FAILURE);
		}

		msg = mmap(NULL, shm_size, PROT_READ, MAP_PRIVATE, r, 0);
		if (msg == MAP_FAILED)  {
			perror("mmap");
			exit(EXIT_FAILURE);
		}

		memcpy(data, msg, shm_size);

		munmap(msg, shm_size);

		close(r);

		print_msg("%d:%s\n", shmid, data);
	} else {
		/* action == READ */
		int i;

		for (i = 0; i < nb_loops; i++) {
			int n = 0;

			print_msg("%s\n", data);
			do_one_loop(i, &n);
#ifdef KTP_TEST
			if (strncmp(data, "KTP REQ CHANGE", 15) == 0)
				strncpy(data, "KTP CHANGE DONE", shm_size);
#endif
		}
	}


	if (shmdt(data) == -1) {
		perror("shmdt");
		exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);
}
