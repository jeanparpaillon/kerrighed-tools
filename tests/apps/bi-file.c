#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "libbi.h"

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
  int i, n;

  printf ("-- Enter bi --\n");

  parse_args(argc, argv);
  
  FILE *f = fopen("/tmp/monfichier", "w");

  n = 0;
  for (i = 0; numloops < 0 || i < numloops; i++)
    {
      do_one_loop(i, &n);
      fprintf(f, "%d\n", i);
      fflush(f);
    }

  fclose(f);
  return 0;
}
