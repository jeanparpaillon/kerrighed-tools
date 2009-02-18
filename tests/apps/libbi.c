#include "libbi.h"
#include <stdio.h>

void do_one_loop(int i, int * n){
  int j;
  for (j = 0; j < 100000000; j++)
    (*n) = (*n) + i * j ;
}
