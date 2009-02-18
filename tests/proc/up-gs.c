#include <stdio.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "test-tools.h"

#define MAXCOLS 2048 
#define MAXROWS 2048
#define ERROR 1e-14

#define MAX(a,b) ((a)>(b) ? (a) : (b))
#define TAILLE_MAT 10

typedef double matrix_t[MAXCOLS][MAXROWS];
typedef double matrix2_t[TAILLE_MAT][TAILLE_MAT];

matrix_t mat;
matrix2_t mat2;

int numcols = 512;
int numrows = 512;
int numloops = 1;
int verbose = 0;
int save_matrix = 0;



/********** matrix functions **********/
void print_matrix(matrix_t * matp)
{
  int i, j;
    
  for (j=0 ; j<numrows ; j++) {
    printf("[");
    for (i=0 ; i<numcols ; i++ )   
      printf ("%g  ", (*matp)[i][j]);
    printf("]\n");
  }
}

void mat_init2()
{
    int i, j;

    for (i=0 ; i<TAILLE_MAT ; i++)
        for (j=0 ; j<TAILLE_MAT ; j++)
	  mat2[i][j] = j;
}

void mat_init(matrix_t * matp)
{
    int i, j;

    if (verbose >= 2) {
        printf("Initializing the matrix...");
        fflush(stdout);
    }
    for (i=0 ; i<numcols ; i++)
      {
        for (j=0 ; j<numrows ; j++)
	  if (i != j)
	    (*matp)[i][j] = sin((double) abs(i-j));
	  else
	    (*matp)[i][j] = numrows;
      }

    if (verbose >= 2)
      printf(" ok\n");
    if (verbose >= 3)
        print_matrix(matp);
}

double orth_err(matrix_t * matp)
{
    int i, j, k;
    double err, s;
    
    if (verbose >= 2) {
        printf("Computing orthogonality error...");
        fflush(stdout);
    }
    err = 0.0;
    for (j=0 ; j<numcols ; j++) {
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
        printf(" ok: %g\n", err);
    return err;
}

void parse_args(int argc, char *argv[])
{
    int c;
    
    while (1){
        c = getopt(argc, argv, "c:r:l:v:hsp");
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
        case 's':
             save_matrix = 1;
             break;
        default:
            printf("** unknown option\n");
        case 'h':
            printf("usage: %s [-h] [-v level] [-c cols] [-r rows] [-l loops]\n", argv[0]);
            exit(1);
        }
    }
}

void up_mgs(matrix_t * matp)
{
    int i, j, k;
    double tmp, xnorm, comp;

    if (verbose >= 2) {
        printf("Computing gram-schmidt...");
        fflush(stdout);
    }

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
        printf(" ok\n");
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

    parse_args(argc, argv);

    printf ("-- Enter MGS %dx%d test --\n", numcols, numrows);
    fflush(stdout);
    if (verbose > 0)
        printf(": see later (debug on)\n");

    time = 0 ;
    for (n=0 ; n<numloops ; n++) {        
        if (verbose >= 1) {
            printf("\r%d", n);
            fflush(stdout);
        }
/*  	timer_start(&start); */
        mat_init(&mat);
		
	timer_start(&start);
	up_mgs(&mat);
	time += timer_stop(&start);

        err = orth_err(&mat);

       if (verbose >= 3)
            print_matrix(&mat);
    }

    printf ("\n   Err = %g, time = %g\n", err, time/(float)numloops);
    if ( err < ERROR )
      printf ("-- MGS test : PASSED --\n");
    else
      {
	printf ("-- MGS test : FAILED --\n");
	exit(-1) ;
      }

    if (save_matrix)
         save_normalized_matrix();

    printf ("up_gs termine\n");
    
    return 0;
}



