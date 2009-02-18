#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include "libbi.h"

int numloops = -1 ;
int quiet = 0;
int close_stdbuffers = 0;

void parse_args(int argc, char *argv[])
{
	int c;
    
	while (1){
		c = getopt(argc, argv, "l:qhc");
		if (c == -1)
			break;
		switch (c) {
		case 'l':
			numloops = atoi(optarg);
			break;
		case 'q':
			quiet = 1;
			break;
		case 'c':
			quiet = 1;
			close_stdbuffers = 1;
			break;
		default:
			printf("** unknown option\n");
		case 'h':
			printf("usage: %s [-h] [-l N] [-q] [-c]\n", argv[0]);
			printf(" -h   : this help\n");
			printf(" -l N : number of loops\n");
			printf(" -q   : quiet\n");
			printf(" -c   : quiet + close the stdin, stdout, "
			       "and stderr\n");
			exit(1);
		}
	}
}


int main(int argc, char *argv[])
{
	int i, n;
	
	parse_args(argc, argv);

	if (!quiet)
		printf ("-- Enter bi (%d) --\n", getpid());
	else if (close_stdbuffers) {
		fclose(stdin);
		fclose(stdout);
		fclose(stderr);
	}
	
	n = 0;
	for (i = 0; numloops < 0 || i < numloops; i++)
	{
		do_one_loop(i, &n);
		if (!quiet)
			printf("%d\n", i);
	}

	if (!quiet)
		printf("-- End of bi (%d) with %d loops --\n", getpid(), i);

	return 0;
}
