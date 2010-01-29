#include <errno.h>
#include <sched.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syscall.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <unistd.h>
#include "libbi.h"

enum operation{
	ADD,
	SHOW,
	TESTLOOP,
	NOTHING
};

enum operation todo = NOTHING;
int create = 0;
int nbmembers = 3;
int member = 0;
int delta = 0;
int use_id = 0;
int quiet = 0;
int blocking = 0;
int undo = 0;

union semun {
	int              val;    /* Value for SETVAL */
	struct semid_ds *buf;    /* Buffer for IPC_STAT, IPC_SET */
	unsigned short  *array;  /* Array for GETALL, SETALL */
	struct seminfo  *__buf;  /* Buffer for IPC_INFO
				    (Linux specific) */
};

void print_msg(const char *format, ...)
{
	if (quiet)
		return;

	va_list params;
	va_start(params, format);
	vprintf(format, params);
	va_end(params);
}

key_t get_key(const char *path)
{
	key_t key;
	key = ftok(path, 'R');
	if (key == -1)
		perror("ftok");

	return key;
}

/* return -1 in case of error */
int create_sem(const char *path, int nbsems)
{
	key_t key;
	int semid = -1;

	key = get_key(path);
	if (key == -1)
		return -1;

	semid = semget(key, nbsems, IPC_CREAT | IPC_EXCL | 0600);
	if (semid == -1)
		fprintf(stderr, "create_sem(%s)::semget: %s\n", path,
			strerror(errno));
	else
		print_msg("%d:\n", semid);

	return semid;
}

/* return -1 in case of error */
int get_sem(const char *path)
{
	key_t key;
	int semid = -1;

	key = get_key(path);
	if (key == -1)
		return -1;

	semid = semget(key, 0, 0);
	if (semid == -1)
		fprintf(stderr, "get_sem(%s)::semget: %s\n", path,
			strerror(errno));

	return semid;
}

int delete_sem(int semid)
{
	int r;
	if (semid == -1) {
		fprintf(stderr, "delete_sem(%d)::semctl: invalid id: %s\n", semid,
			strerror(-EINVAL));
		return -EINVAL;
	}

	r = semctl(semid, 0, IPC_RMID, NULL);
	if (r)
		fprintf(stderr, "delete_sem(%d)::semctl: %s\n", semid,
			strerror(errno));

	return r;
}

int add_to_member(int semid, int member, int blocking, int undo, int delta)
{
	int r;
	struct sembuf sem_op = {member, delta, 0};

	if (!blocking)
		sem_op.sem_flg |= IPC_NOWAIT;
	if (undo)
		sem_op.sem_flg |= SEM_UNDO;

	r = semop(semid, &sem_op, 1);
	if (r)
		fprintf(stderr, "add_to_member(%d)::semop: %s\n", semid,
			strerror(errno));

	return r;
}

unsigned short count_sem_members(int semid)
{
	int r;
	union semun semopts;
	struct semid_ds mysemds;

	semopts.buf = &mysemds;

	r = semctl(semid, 0, IPC_STAT, semopts);
	if (r) {
		fprintf(stderr, "count_sem_members(%d)::semctl: %s\n", semid,
			strerror(errno));
		return r;
	}

	return semopts.buf->sem_nsems;
}

int get_val_sem_member(int semid, int member)
{
	int semval;
	semval = semctl(semid, member, GETVAL, 0);
	return semval;
}

int display_sem(int semid)
{
	unsigned short nsems, i;

	nsems = count_sem_members(semid);
	if (nsems < 0)
		return nsems;

	printf("%d: (%d)", semid, nsems);
	for (i = 0; i < nsems; i++) {
		int semval = get_val_sem_member(semid, i);
		printf("%d ", semval);
	}
	printf("\n");
	return 0;
}

int test_loop(int semid, int undo)
{
	unsigned short nsems, i;
	int op, r = 0;
	pid_t pid;

	nsems = count_sem_members(semid);
	if (nsems < 0)
		return nsems;

	for (i = 0; i < nsems; i++) {
		r = add_to_member(semid, i, 1, undo, rand()%10);
		if (r)
			return r;
	}

	/* Now forking, the child will lock while the parent will unlock */

	pid = (pid_t)syscall(SYS_clone, SIGCHLD | CLONE_SYSVSEM, NULL);
	if (pid == -1) {
		perror("clone");
		exit(EXIT_FAILURE);

	} else if (pid == 0)
		op = 1;
	else
		op = -1;

	while (1) {
		int n;

		for (i = 0; i < nsems; i++) {
			r = add_to_member(semid, i, 1, undo, op);
			if (r)
				return r;
		}

		do_one_loop(rand()%10, &n);
	}

	return r;
}


void print_usage()
{
	printf("Usage: ipcsem-tool OPERATIONS [OPTIONS] path\n"
	       "* Operations\n"
	       " -h                     : show this help\n"
	       " -c nb_members          : create a semaphore\n"
	       " -d                     : delete the semaphore\n"
	       " -a member:delta        : add value delta to a member of the semaphore array\n"
	       " -l member              : lock a member of the semaphore array\n"
	       " -u member              : unlock a member of the semaphore array\n"
	       " -s                     : show the values of members\n"
	       "* Various options\n"
	       " -i                     : use semaphore identifier instead of path\n"
	       " -q                     : be quiet\n"
	       " -U                     : mark operations as undoable\n"
	       " -b                     : mark operations as blocking\n"
		);
}

int parse_member_delta(char *arg)
{
	int r;
	r = sscanf(arg, "%d:%d", &member, &delta);
	if (r == 2)
		return 0;

	fprintf(stderr, "Invalid argument: %s\n", arg);
	return -EINVAL;
}

void parse_args(int argc, char *argv[])
{
	int c;

	while (1) {

		c = getopt(argc, argv, "hc:da:l:u:siqULb");
		if (c == -1)
			break;

		switch (c) {
		case 'h':
			print_usage();
			exit(EXIT_SUCCESS);
			break;
		case 'c':
			create = 1;
			nbmembers = atoi(optarg);
			break;
		case 'd':
			create = -1;
			break;
		case 'a':
			todo = ADD;
			if (parse_member_delta(optarg) != 0) {
				print_usage();
				exit(EXIT_FAILURE);
			}
			break;
		case 'l':
			todo = ADD;
			member = atoi(optarg);
			delta = -1;
			break;
		case 'u':
			todo = ADD;
			member = atoi(optarg);
			delta = 1;
			break;
		case 's':
			todo = SHOW;
			break;
		case 'i':
			use_id = 1;
			break;
		case 'q':
			quiet = 1;
			break;
		case 'U':
			undo = 1;
			break;
		case 'L':
			todo = TESTLOOP;
			undo = 1;
			break;
		case 'b':
			blocking = 1;
			break;
		default:
			exit(EXIT_FAILURE);
			break;
		}
	}
}


int main(int argc, char* argv[])
{
	int r = 0, semid;

	parse_args(argc, argv);

	if (argc - optind != 1) {
		print_usage();
		exit(EXIT_FAILURE);
	}

	if (use_id && create == 1) {
		fprintf(stderr, "** incompatible options used: -c and -i\n");
		exit(EXIT_FAILURE);
	}

	if (create == 1) {
		semid = create_sem(argv[optind], nbmembers);
		if (semid == -1)
			r = semid;
	} else {
		if (use_id) {
			semid = atoi(argv[optind]);
		} else {
			semid = get_sem(argv[optind]);
		}

		if (create == -1) { /* user wants to remove the SEM object */
			int r;
			r = delete_sem(semid);
			if (r)
				exit(EXIT_FAILURE);

			exit(EXIT_SUCCESS);
		}
	}

	if (semid == -1)
		exit(EXIT_FAILURE);

	switch (todo) {
	case ADD:
		r = add_to_member(semid, member, blocking, undo, delta);
		break;
	case SHOW:
		r = display_sem(semid);
		break;
	case TESTLOOP:
		r = test_loop(semid, undo);
		break;
	case NOTHING:
		if (create == 0) {
			print_usage();
			r = -1;
		}
		break;
	}

	if (r)
		exit(EXIT_FAILURE);

	exit(EXIT_SUCCESS);
}
