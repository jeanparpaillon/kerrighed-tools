#include <sys/time.h>

#define MASTER 0


/********** timing **********/

struct timeval start;
float tim = 0.0;


/****************************** Test tools ********************************/



static inline void print_progress (int thr_id, int index, int max)
{
  int step = max / 10 ;

  if (thr_id != MASTER)
    return ;

  if (index == 0)
    {
      printf ("   Progress : ");
      fflush (stdout);
    }
  
  if ( index % step == 0 )
    {
      printf ("#");
      fflush(stdout);
    }
}



/****************************** timing **********************************/



static inline void timer_start(struct timeval * tv)
{
    gettimeofday(tv, NULL);
}

static inline float timer_stop(struct timeval * start)
{
    struct timeval stop;
    
    gettimeofday(&stop, NULL);
    return ((float)(stop.tv_sec - start->tv_sec) + 1e-6*(float)(stop.tv_usec - 
start->tv_usec));
}
