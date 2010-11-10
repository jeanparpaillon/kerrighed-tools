#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

int numloops = -1 ;
int quiet = 0;

#define TABLE_SIZE 1000000
int big_table[TABLE_SIZE];

void parse_args(int argc, char *argv[])
{
	int c;

	while (1){
		c = getopt(argc, argv, "l:qh");
		if (c == -1)
			break;
		switch (c) {
		case 'l':
			numloops = atoi(optarg);
			break;
		case 'q':
			quiet = 1;
			break;
		default:
			printf("** unknown option\n");
		case 'h':
			printf("usage: %s [-h] [-l N] [-q]\n", argv[0]);
			printf(" -h   : this help\n");
			printf(" -l N : number of processes to create\n");
			printf(" -q   : quiet\n");
			exit(1);
		}
	}
}


int main(int argc, char *argv[])
{
	int i, status;

	parse_args(argc, argv);

	memset(big_table, 2, TABLE_SIZE * sizeof(int));

	for (i = 0; numloops < 0 || i < numloops; i++)
	{
		pid_t pid = fork();
		if (pid == -1) {
			perror("fork");
			exit(EXIT_FAILURE);
		}

		if (pid == 0) {
			/* we are in the child process */
			exit(EXIT_SUCCESS);
		} else {
			if ((i+1) % 1000 == 0)
				if (!quiet)
					printf("%d forks done\n", i+1);
			wait(&status);
		}
	}

	if (!quiet)
		printf("%d forks done\n", numloops);

	return 0;
}
