/*
 *  Copyright (C) 2001-2006, INRIA, Universite de Rennes 1, EDF.
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>

#include <kerrighed.h>

int pid, nodeid ;

void parse_args(int argc, char *argv[])
{
  int c;
  
  while (1)
    {
      c = getopt(argc, argv, "h");
      if (c == -1)
	break;

      switch (c)
	{
          default:
	    printf("** unknown option\n");
	  case 'h':
	    goto err;
        }
    }
  
  if (optind != argc - 2)
    goto err ;
  
  pid = atoi(argv[optind++]);
  nodeid = atoi(argv[optind++]);
    
  return ;

err:
    printf ("usage: %s [-h] pid nodeid\n", argv[0]);
    printf ("  -h : This help\n");
    exit(1);
}


int main(int argc, char *argv[])
{
  int r ;

  parse_args(argc, argv);
 
  r = migrate (pid, nodeid);

  if (r != 0)
    {
      perror("migrate");
      return 1 ;
    }

  return 0;
}
