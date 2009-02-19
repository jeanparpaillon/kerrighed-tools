#include <stdio.h>
#include <sys/types.h>
#include <syscall.h>
#include <unistd.h>

#include "libbi.h"

void do_one_loop(int i, int * n)
{
  int j;
  for (j = 0; j < 100000000; j++)
    (*n) = (*n) + i * j ;
}

void __do_all_loops(int quiet, int *i, int numloops)
{
	pid_t pid, tid;
	int n;

	pid = getpid();
	tid = (pid_t)syscall(SYS_gettid);

	if (!quiet)
		printf("-- Enter bi (%d-%d) --\n", pid, tid);

	n = 0;
	for (; numloops < 0 || *i < numloops; (*i)++) {
		do_one_loop(*i, &n);
		if (!quiet)
			printf("(%d-%d) %d\n", pid, tid, *i);
	}

	if (!quiet)
		printf("-- End of bi (%d-%d) with %d loops --\n", pid, tid, *i);
}

void do_all_loops(int quiet, int numloops)
{
	int i = 0;

	__do_all_loops(quiet, &i, numloops);
}

void close_sync_pipe()
{
	close(4);
}

void close_stdioe(int closing)
{
	if (closing) {
		fclose(stdin);
		fclose(stdout);
		fclose(stderr);
	}
}

