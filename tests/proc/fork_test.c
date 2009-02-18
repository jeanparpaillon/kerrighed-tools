#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>


int main()
{
  int i, j, k, pid, fpid, status, dummy ;
  int sonlist[4] ;

  printf ("-- Enter distant fork test --\n");

  fpid = getpid();

  for (i=0; i < 4; i++)
    {
      pid = fork() ;
      if (pid)
	{
	  sonlist[i] = pid ;
	}
      else
	{
	  for (j = 0; j <=i; j++)
	    sonlist[j] = 0 ;
	}
      printf ("#") ;
      fflush(stdout) ;

      dummy = 0 ;
      for (j = 0; j < 30000; j++)
	for (k = 0; k < 10000; k++)
	  dummy += j * k + i * dummy ;
    }

  for (i = 0; i < 4; i++)
    {
      if (sonlist[i] != 0)
	waitpid(sonlist[i], &status, 0);
    }

  if (fpid == getpid())
    printf ("\n-- Distant fork test : PASSED --\n\n\n");

  return 0 ;
}
