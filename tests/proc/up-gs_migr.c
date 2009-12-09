#include <stdio.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/time.h>
#include <unistd.h>


#include <test-tools.h>
#include <kerrighed.h>

#define MAXCOLS 2048
#define MAXROWS 2048
#define ERROR 1e-14

#define MAX(a,b) ((a)>(b) ? (a) : (b))
#define TAILLE_MAT 10

typedef double matrix_t[MAXCOLS][MAXROWS];

matrix_t mat;

/* computation parameters */
int numcols = 500;
int numrows = 500;
int numloops = 1;
int verbose = 0;
int save_matrix = 0;

int migration = 1 ;
int use_ctnr = 0 ;
int nr_migrations = 2 ;
int next_node ;
int nr_nodes, initial_node ;

static unsigned int checksum(void* _buffer, size_t length){
  register unsigned int res;
  unsigned int *buffer = _buffer;
  unsigned char tps[sizeof(unsigned int)];
  size_t i;
  
  // TODO: take care of the int-alignment
  
  //printk("===================\n");
  res = 0;
  if(length >= sizeof(unsigned int)){
    for(i=0;i*sizeof(unsigned int) <= length-sizeof(unsigned int);i++){
      res ^= buffer[i];
    };
  }else{
    i = 0;
  };
  
  if(i*sizeof(unsigned int) != length ){
    tps[0] = ((unsigned char*)buffer)[i*sizeof(unsigned int)];
    if(i*sizeof(unsigned int)+1 < length){
      tps[1] = ((unsigned char*)buffer)[i*sizeof(unsigned int)+1];
    }else{
      tps[1] = 0;
    };
    if(i*sizeof(unsigned int)+2 < length){
      tps[2] = ((unsigned char*)buffer)[i*sizeof(unsigned int)+2];
    }else{
      tps[2] = 0;
    };
    tps[3] = 0;
    
    res ^= *(unsigned int*)(tps);
  };
  
  return res;
};

static inline unsigned int control_vars()
{
  unsigned int res ;
  printf("%d %d %d %d %d ", numcols, numrows, numloops, verbose, save_matrix) ;
  printf("%d %d %d %d %d %d ", migration, use_ctnr, nr_migrations, next_node, nr_nodes, initial_node) ;
  res = checksum (mat, sizeof (mat)) ;
  printf("%d mat checksum on size %zd\n", res , sizeof(mat)) ;
  return res ;
}

/********** matrix functions **********/

void mat_init(matrix_t * matp)
{
    int i, j;

    if (verbose >= 2) {
      printf("Initializing the matrix...");
      fflush(stdout);
    }
    for (i=0 ; i<numcols ; i++)
        for (j=0 ; j<numrows ; j++)
            if (i != j)
                (*matp)[i][j] = sin((double) abs(i-j));
            else
                (*matp)[i][j] = numrows;
    if (verbose >= 2)
        printf(" ok\n");
}

double orth_err(matrix_t * matp)
{
  int i, j, k;
  double err, s;
  
  if (verbose >= 2)
    {
      printf("Computing orthogonality error...");
      fflush(stdout);
    }
  
  err = 0.0;
  
  for (j=0 ; j<numcols ; j++)
    {
      print_progress (0, numcols + j, 2*numcols);
      s = 0.0;
      for (k=0 ; k<numrows ; k++)
	s += (*matp)[j][k] * (*matp)[j][k];
      s -= 1;
      err = MAX(err, fabs(s));
      
      for (i=j+1 ; i<numcols ; i++) {
	s = 0.0;
	for (k=0 ; k<numrows ; k++)
	  s += (*matp)[i][k] * (*matp)[j][k];
	err = MAX(err, fabs(s));
      }
    }
  
  if (verbose >= 2)
    printf(" ok: %g, verbose = %d\n", err, verbose);
  
  return err;
}



void parse_args(int argc, char *argv[])
{
  int c;
  
  while (1)
    {
      c = getopt(argc, argv, "c:r:l:v:m:hsCp");
      if (c == -1)
	break;

      switch (c) {
        case 'p':
	  printf("The pid of the process is %d\n", getpid());
	  break;
        case 'c':
	  numcols = atoi(optarg);
	  break;
        case 'r':
	  numrows = atoi(optarg);
	  break;
        case 'l':
	  numloops = atoi(optarg);
	  break;
        case 'v':
	  verbose = atoi(optarg);
	  break;
        case 'm':
	  nr_migrations = atoi(optarg);
	  break;
        case 's':
	  save_matrix = 1;
	  break;
        case 'C':
	  use_ctnr = 1 ;
	  break;
        default:
	  printf("** unknown option\n");
        case 'h':
	  printf("usage: %s [-h] [-v level] [-c cols] [-r rows] [-l loops] [-m number of migrations] [-C]\n",
		 argv[0]);
	  exit(1);
      }
    }
}



/***************************** Main MGS code *******************************/

void up_mgs(matrix_t * matp)
{
  int i, j, k, migration_gap;
  double tmp, xnorm, comp;
    
  if (verbose >= 2)
    {
      printf("Computing gram-schmidt...");
      fflush(stdout);
    }
  
  migration_gap = (numcols / (nr_migrations+1) +1);
  
  if (use_ctnr)
    printf ("-- Enter migration test with containers (MGS %dx%d) --\n",
	    numcols, numrows);
  else
    printf ("-- Enter migration test (%d migrations) without containers (MGS %dx%d) --\n",
	    nr_migrations, numcols, numrows);

  for (i=0 ; i<numcols ; i++)
    {
      print_progress (0, i, 2*numcols);
      
      /* Normalize the vector */
      tmp = 0.0;
      if (verbose >= 2)
	printf("Start normalization\n");
      
      for (k=0 ; k<numrows ; k++)
	tmp += (*matp)[i][k] * (*matp)[i][k];
      xnorm = 1.0/sqrt(tmp);
      for (k=0 ; k<numrows ; k++)
	(*matp)[i][k] *= xnorm;

      if (migration == 1 && i != 0 &&
	  (i % migration_gap == 0) )
	{
	  //unsigned int res ;
	  //res = control_vars() ;
	  //printf("checksum is %u at step %d\n", res, i) ; 
	  migrate_self (next_node);

	  //res = control_vars() ;
	  //printf("checksum is %u at step %d\n", res, i) ; 
	  next_node = (next_node + 1) % nr_nodes ;
	}
      
      /* propagate result */
      for (j=i+1 ; j<numcols ; j++)
	{
	  comp = 0.0;
	  for (k=0 ; k<numrows ; k++)
	    comp += (*matp)[i][k] * (*matp)[j][k];
	  	  
	  comp = -comp;
	  for (k=0 ; k<numrows ; k++)
	    (*matp)[j][k] += comp * (*matp)[i][k];
	}
    }
  
  if (verbose >= 2)
    printf(" ok, verbose == %d\n", verbose);
}



void save_normalized_matrix(void)
{
     int i, j;
     FILE *fd;
     char str[125];
     
     sprintf(str, "mat-gs-up-%d-%d", numcols, numrows);
     fd = fopen(str, "w");
     if (fd == NULL) {
          perror("save matrix, open file");
          return;
     }
     for (i=0 ; i<numcols ; i++) {
          for (j=0 ; j<numrows ; j++)
               fprintf(fd, "%g ", mat[i][j]);
          fprintf(fd, "\n");
     }
     fclose(fd);
}



int main(int argc, char *argv[])
{
    double err = 0.0;
    struct timeval start;
    float time = 0.0;
    int n;

    nr_nodes =  get_nr_nodes();
    initial_node = get_node_id () ;
    next_node = (initial_node + 1) % nr_nodes ;
    nr_migrations = nr_nodes + 1 ;

    parse_args(argc, argv);

    if (verbose>=2)
      {
	printf("gram-schmidt 'uni-proc' for %dx%d in", numcols, numrows);
	fflush(stdout);
      }

    time = 0 ;
    for (n=0 ; n<numloops ; n++)
      {        
	mat_init(&mat);
	
	timer_start(&start);
	up_mgs(&mat);
	time += timer_stop(&start);
	if (verbose > 2)
	  printf ("up_gs finished, starting orth_err\n");

	if (migration == 1 &&
	    get_node_id() != initial_node)
	  migrate_self (initial_node);
	
	err = orth_err(&mat);
	printf ("\n   Err = %g\n", err);
	if ( err < ERROR )
	  printf ("-- MGS test : PASSED --\n\n\n");
	else
	  {
	    printf ("-- MGS test : FAILED --\n\n\n");
	    exit(-1) ;
	  }
      }

    /* migration ici = pb */

    if (verbose>2)
      printf ("up-gs : terminé\n");

    if (verbose >= 1)
      {
        printf("\ntime :");
	printf(" %g s (err: %g)\n", time/(float)numloops, err);
      }

    if (save_matrix)
         save_normalized_matrix();

    if (verbose>2)
      printf ("up_gs termine\n");
    
    return 0;
}
