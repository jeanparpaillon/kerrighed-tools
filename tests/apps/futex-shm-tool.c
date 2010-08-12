/*
 * Copyright (C) 2004 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * Copyright (C) 2010 Kerlabs - Matthieu Fertré
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <asm/unistd.h>
#include <linux/futex.h>
#include <sys/time.h>

static inline int futex(int *uaddr, int op, int val,
			const struct timespec *utime, int *uaddr2, int val3)
{
	return syscall(__NR_futex, uaddr, op, val, utime, uaddr2, val3);
}

#define SYSERROR(X, Y)					\
do {							\
	if ((long)(X) == -1L) {				\
		perror(Y);				\
		exit(EXIT_FAILURE);			\
	}						\
} while(0)

int shmkey = 23;
int shmkey2 = 24;
int nr_wake = 1;
int nr_requeue = 1;
int bitset = 0;
int quiet = 0;
struct timespec utime;

void print_usage(const char* cmd)
{
	printf("%s -h: show this help\n", cmd);
}

void dowait(void)
{
	int shmid, ret, *f, n;
	struct timespec *timeout = NULL;

	shmid = shmget(shmkey, 4, IPC_CREAT|0666);
	SYSERROR(shmid, "shmget");

	f = shmat(shmid, NULL, 0);
	SYSERROR(f, "shmat");

	n = *f;

	if (utime.tv_sec)
		timeout = &utime;

	if (bitset) {
		if (!quiet)
			printf("WAIT_BITSET: %p{%x} bits: %x\n", f, n, bitset);
		ret = futex(f, FUTEX_WAIT_BITSET, n, timeout, NULL, bitset);
	} else {
		if (!quiet)
			printf("WAIT: %p{%x}\n", f, n);
		ret = futex(f, FUTEX_WAIT, n, timeout, NULL, 0);
	}
	SYSERROR(ret, "futex_wait");
	if (!quiet)
		printf("WAITED: %d\n", ret);

	ret = shmdt(f);
	SYSERROR(ret, "shmdt");
}

int dowake(void)
{
	int shmid, ret, *f, nr_proc;

	shmid = shmget(shmkey, 4, IPC_CREAT|0666);
	SYSERROR(shmid, "shmget");

	f = shmat(shmid, NULL, 0);
	SYSERROR(f, "shmat");

	(*f)++;

	if (bitset) {
		if (!quiet)
			printf("WAKE_BITSET: %p{%x} bits: %x\n", f, *f, bitset);
		ret = futex(f, FUTEX_WAKE_BITSET, nr_wake, NULL, NULL, bitset);
	} else {
		if (!quiet)
			printf("WAKE: %p{%x}\n", f, *f);
		ret = futex(f, FUTEX_WAKE, nr_wake, NULL, NULL, 0);
	}

	SYSERROR(ret, "futex_wake");
	if (!quiet)
		printf("WOKE: %d\n", ret);
	nr_proc = ret;

	ret = shmdt(f);
	SYSERROR(ret, "shmdt");

	if (!ret)
		ret = nr_proc;

	return ret;
}

int dorequeue(void)
{
	int shmid1, shmid2, ret, *f1, *f2, nr_proc;

	shmid1 = shmget(shmkey, 4, IPC_CREAT|0666);
	SYSERROR(shmid1, "shmget");

	shmid2 = shmget(shmkey2, 4, IPC_CREAT|0666);
	SYSERROR(shmid2, "shmget");

	f1 = shmat(shmid1, NULL, 0);
	SYSERROR(f1, "shmat");

	f2 = shmat(shmid2, NULL, 0);
	SYSERROR(f2, "shmat");

	/* requeue */
	ret = futex(f1, FUTEX_REQUEUE, nr_wake,
		    (const struct timespec *) (long) nr_requeue, f2, 0);
	SYSERROR(ret, "futex_requeue");

	if (!quiet)
		printf("WOKE or REQUEUED: %d\n", ret);
	nr_proc = ret;

	/* detaching shms */
	ret = shmdt(f1);
	SYSERROR(ret, "shmdt");

	ret = shmdt(f2);
	SYSERROR(ret, "shmdt");

	if (!ret)
		ret = nr_proc;

	return ret;
}

void badfutex(void)
{
	int *x;
	int ret;

	x = mmap(NULL, 16384, PROT_READ, MAP_PRIVATE|MAP_ANON, -1, 0);
	SYSERROR(x, "mmap");

	ret = futex(x, FUTEX_WAIT, 0, NULL, NULL, 0);
	SYSERROR(ret, "futex");
}

void deletekey(void)
{
	int shmid, ret;

	shmid = shmget(shmkey, 4, 0666);
	SYSERROR(shmid, "shmget");

	ret = shmctl(shmid, IPC_RMID, NULL);
	SYSERROR(ret, "shmctl(IPC_RMID)");

	if (!quiet)
		printf("SHM %d (id=%d) DELETED\n", shmkey, shmid);
}

void parse_args(int argc, char *argv[])
{
	int c;

	utime.tv_sec = 0;
	utime.tv_nsec = 0;

	while (1) {
		c = getopt(argc, argv, "hqb:k:K:r:t:w:");
		if (c == -1)
			break;

		switch (c) {
		case 'h':
			print_usage(argv[0]);
			exit(EXIT_SUCCESS);
			break;
		case 'q':
			quiet=1;
			break;

		case 'b':
			bitset = atoi(optarg);
			break;
		case 'k':
			shmkey = atoi(optarg);
			break;
		case 'K':
			shmkey2 = atoi(optarg);
			break;
		case 'r':
			nr_requeue = atoi(optarg);
			break;
		case 't':
			utime.tv_sec = atoi(optarg);
			break;
		case 'w':
			nr_wake = atoi(optarg);
			break;
		}
	}
}

int main(int argc, char **argv)
{
	char *action;
	int ret = 0;

	parse_args(argc, argv);

	if (argc - optind == 0) {
		print_usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	action = argv[optind];

	if (!quiet)
		printf("Command: %s\n", action);

	if (strcmp(action, "badfutex") == 0)
		badfutex();

	else if (strcmp(action, "wait") == 0)
		dowait();

	else if (strcmp(action, "wake") == 0)
		ret = dowake();

	else if (strcmp(action, "requeue") == 0)
		ret = dorequeue();

	else if (strcmp(action, "delete") == 0)
		deletekey();

	else {
		fprintf(stderr, "Unknown command\n");
		print_usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	exit(ret);
}
