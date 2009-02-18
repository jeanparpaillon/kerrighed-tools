#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int numloops = -1 ;

void parse_args(int argc, char *argv[])
{
    int c;
    
    while (1){
        c = getopt(argc, argv, "l:h");
        if (c == -1)
            break;
        switch (c) {
        case 'l':
            numloops = atoi(optarg);
            break;
        default:
            printf("** unknown option\n");
        case 'h':
            printf("usage: %s [-h] [-l N]\n", argv[0]);
	    printf(" -h   : this help\n");
	    printf(" -l N : number of loops\n");
            exit(1);
        }
    }
}


int main(int argc, char *argv[])
{
  int i, j, n;

  printf ("-- Enter bi --\n");

  parse_args(argc, argv);
  
  for (i = 0; numloops < 0 || i < numloops; i++)
    {
      for (j = 0; j < 100000000; j++)
	n = n + i * j ;
    }

  return 0;
}
