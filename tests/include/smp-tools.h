#ifndef SMP_TOOLS_H

#define SMP_TOOLS_H

#include <pthread.h>
#include <semaphore.h>

#define PTHREAD_BARRIER_SERIAL_THREAD 1
#define DEFAULT_NB_NODES 2
#define pthread_init(a,b)

typedef struct barrier {
  pthread_mutex_t mylock ;
  int count;
  int nr_cpu;
  pthread_cond_t cond ;
} pthread_barrier_t ;

typedef int pthread_barrierattr_t;

static inline int pthread_nr_cpu()
{
  return DEFAULT_NB_NODES;
}

#define krg_mem_prefetch(A,B,C) do{}while(0)


static inline int pthread_barrier_init(pthread_barrier_t *bar,
				       pthread_barrierattr_t *attr,
				       unsigned long nr_thr)
{
  bar->count = 0;
  bar->nr_cpu = nr_thr;

  pthread_mutex_init ( &(bar->mylock) , NULL ) ;
  pthread_cond_init ( &(bar->cond) , NULL ) ;

  return 0 ;
}


static inline int pthread_barrier_wait(pthread_barrier_t *bar)
{
  int r = 0 ;
  int N = bar->nr_cpu ;
  pthread_mutex_lock ( &(bar->mylock) ) ;

  if (bar->count < N-1)
    {
      bar->count++;
      pthread_cond_wait ( &(bar->cond),
                          &(bar->mylock) ) ;

      pthread_mutex_unlock ( &(bar->mylock) ) ;
    }
  else
    {
      pthread_mutex_unlock ( &(bar->mylock) ) ;
      bar->count = 0;
      r = PTHREAD_BARRIER_SERIAL_THREAD ;
      pthread_cond_broadcast ( &(bar->cond) ) ;
    }

  return r ;
}

#endif // SMP_TOOLS_H
