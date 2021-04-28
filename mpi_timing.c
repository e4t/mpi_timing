/*
 * Copyright (C) 2021 SUSE
 *
 * This file is part of mpi_timing.
 *
 * Tlog is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpi_timing is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with tlog; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */


#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include <mpi.h>

#include <gsl/gsl_statistics.h>
#include <gsl/gsl_sort.h>

#include "tlog/timespec.h"


static unsigned int fill_random = 0;
static int nr_runs = 1000;
static int world_rank = 0;
static int world_size = 0;

int int_pow(int base, int exp) {
    int result = 1;
    for (;;)
    {
        if (exp & 1)
            result *= base;
        exp >>= 1;
        if (!exp)
            break;
        base *= base;
    }

    return result;
}


void usage() {
  printf("\tUsage: mpi_init [-rh]\n");
  printf("\tperform small MPI timing test\n");
  printf("\t-h print this help\n");
  printf("\t-r initialize data with (pseudo) random values\n");
  printf("\t-s SEED set random seed\n");
  printf("\t-t TIMES how many times to run the test, default is %i\n",nr_runs);
  printf("\n");
  exit(EXIT_SUCCESS);
}

void parse_cmdline(int *argc,char*** argv) {
  int opt = 0;
  srand(42);
  while((opt = getopt(*argc,*argv,"rhs:t:")) != -1 ) {
    switch(opt) {
      case 'r':
        fill_random = 1;
        break;
      case 'h':
        usage();
        break;
      case 's':
        srand(atoi(optarg));
        break;
      case 't':
        nr_runs = (atoi(optarg));
        break;
    }
  }
}

void round_trip(const unsigned int msg_size,struct timespec *snd_time, struct timespec *rcv_time) {
  int * data = calloc(msg_size,sizeof(int));
  int msg_id = 0;
  struct timespec time_start, time_end;
  if(world_rank != 0) {
    clock_gettime(CLOCK_MONOTONIC, &time_start);
    MPI_Recv(data,msg_size,MPI_INT,
        world_rank - 1,msg_id,MPI_COMM_WORLD,MPI_STATUS_IGNORE);
    clock_gettime(CLOCK_MONOTONIC, &time_end);
    tlog_timespec_sub(&time_end,&time_start,rcv_time);
    /*
    printf("# Got following data:");
    for(unsigned int i = 0; i < msg_size;i++)
      printf(" %i",data[i]);
    printf("\n");
    */

  } else {
    if(fill_random) {
      for(unsigned int i = 0; i < msg_size; i++) {
        data[i] = rand();
      }
    }
  }
  clock_gettime(CLOCK_MONOTONIC, &time_start);
  MPI_Send(data,msg_size,MPI_INT,
      (world_rank + 1) % world_size,msg_id,MPI_COMM_WORLD);
  clock_gettime(CLOCK_MONOTONIC, &time_end);
  tlog_timespec_sub(&time_end,&time_start,snd_time );
  if(world_rank == 0) {
    clock_gettime(CLOCK_MONOTONIC, &time_start);
    MPI_Recv(data,msg_size,MPI_INT,
        world_size-1,msg_id,MPI_COMM_WORLD,MPI_STATUS_IGNORE);
    clock_gettime(CLOCK_MONOTONIC, &time_end);
    tlog_timespec_sub(&time_end,&time_start,rcv_time);
  }

  free(data);
}

int main(int argc, char** argv) {
  struct timespec time_start, time_end, time_diff, time_gl_start, time_gl_end,time_gl_diff; 
  parse_cmdline(&argc,&argv);

  clock_gettime(CLOCK_MONOTONIC, &time_gl_start);
  clock_gettime(CLOCK_MONOTONIC, &time_start);
  MPI_Init(&argc,&argv);
  clock_gettime(CLOCK_MONOTONIC, &time_end);
  // Get the number of processes
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);

  MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
  char processor_name[MPI_MAX_PROCESSOR_NAME];
  int name_len;
  MPI_Get_processor_name(processor_name, &name_len);

  /* start with time which was needed for init and some more general information*/
  long* send_bf_init = malloc(2*sizeof(long));
  tlog_timespec_sub(&time_end,&time_start,&time_diff);
  send_bf_init[0] = time_diff.tv_sec; 
  send_bf_init[1] = time_diff.tv_nsec;
  if (world_rank == 0 ) {
    int mpi_version_len = 0;
    char mpi_version[MPI_MAX_LIBRARY_VERSION_STRING];
    MPI_Get_library_version(mpi_version,&mpi_version_len);
    printf("# MPI version: %s\n",mpi_version);
    printf("# Nr of processors are: %i\n",world_size);
    long *recv_bf_init = malloc(2*world_size*sizeof(long));
    char *recv_bf_proc = malloc(world_size*sizeof(char)*MPI_MAX_PROCESSOR_NAME);
    MPI_Gather(send_bf_init,2,MPI_LONG,
        recv_bf_init,2,MPI_LONG,0,MPI_COMM_WORLD);
    /* get the processor (node) names and print them out */
    MPI_Gather(processor_name,MPI_MAX_PROCESSOR_NAME,MPI_CHAR,
        recv_bf_proc,MPI_MAX_PROCESSOR_NAME,MPI_CHAR,0,MPI_COMM_WORLD);
    for(unsigned int i = 0; i < (unsigned int) world_size; i++) {
      char temp_str[MPI_MAX_PROCESSOR_NAME];
      strncpy(temp_str,&recv_bf_proc[MPI_MAX_PROCESSOR_NAME*i],MPI_MAX_PROCESSOR_NAME);
      if(strlen(temp_str) > 0) {
        printf("# %s:",temp_str);
        for(unsigned int j = i; j < (unsigned int) world_size; j++) {
          if(strcmp(temp_str,&recv_bf_proc[MPI_MAX_PROCESSOR_NAME*j])== 0) {
            printf(" %i",j);
            memset(&recv_bf_proc[MPI_MAX_PROCESSOR_NAME*j],'\0',MPI_MAX_PROCESSOR_NAME);
          }
        }
        printf("\n");
      }
    }
    printf("# MPI_Init times for ranks\n");
    for(unsigned int i = 0; i < (unsigned int) world_size; i++) {
      printf("# %lu.%lu\n",recv_bf_init[2*i],recv_bf_init[2*i+1]);
    }

    free(recv_bf_init);
    free(recv_bf_proc);
  } else {
    MPI_Gather(send_bf_init,2,MPI_LONG,NULL,2,MPI_LONG,0,MPI_COMM_WORLD);
    MPI_Gather(processor_name,MPI_MAX_PROCESSOR_NAME,MPI_CHAR,NULL,MPI_MAX_PROCESSOR_NAME,MPI_CHAR,0,MPI_COMM_WORLD);
  }
  free(send_bf_init);

  unsigned int pkg_size = 2;
  for(unsigned int j = 4; j <= 14;) {
    /* not only package size of 2 4 8, but 2 3 4 6 8 ... */
      if(pkg_size <(unsigned int) int_pow(2,j) ) {
        pkg_size = int_pow(2,j);
      } else {
        pkg_size += int_pow(2,j+1); 
        pkg_size /= 2;
        j++;
    }
    double *times_snd = calloc(nr_runs,sizeof(double));
    double *times_rcv = calloc(nr_runs,sizeof(double));
    for(int j = 0; j < nr_runs; j++) {
      /* now start with the ring test */
      struct timespec time_rcv, time_snd; 
      round_trip(pkg_size,&time_rcv,&time_snd);
      times_snd[j] = tlog_timespec_to_fp(&time_snd);
      times_rcv[j] = tlog_timespec_to_fp(&time_rcv);
    }
    gsl_sort(times_snd,1,nr_runs); gsl_sort(times_rcv,1,nr_runs);
    double send_bf[10] = {
        gsl_stats_max(times_snd,1,nr_runs),gsl_stats_min(times_snd,1,nr_runs),
        gsl_stats_mean(times_snd,1,nr_runs),gsl_stats_median_from_sorted_data(times_snd,1,nr_runs),
        gsl_stats_variance(times_snd,1,nr_runs),
        gsl_stats_max(times_rcv,1,nr_runs),gsl_stats_min(times_rcv,1,nr_runs),
        gsl_stats_mean(times_rcv,1,nr_runs),gsl_stats_median_from_sorted_data(times_rcv,1,nr_runs),
        gsl_stats_variance(times_rcv,1,nr_runs) };
    if (world_rank == 0 ) {
      double *recv_bf = malloc(world_size * 10 * sizeof(double));
      clock_gettime(CLOCK_MONOTONIC, &time_start);
      MPI_Gather(send_bf,10,MPI_DOUBLE,recv_bf,10,MPI_DOUBLE,0,MPI_COMM_WORLD);
      clock_gettime(CLOCK_MONOTONIC, &time_end);
      tlog_timespec_sub(&time_end,&time_start,&time_diff);
      printf("# Time for gather %lu.%lu\n",time_diff.tv_sec,time_diff.tv_nsec);
      printf("%i %g %g %g %g %g %g %g %g %g %g \n",pkg_size,
          recv_bf[0],recv_bf[1],recv_bf[2],recv_bf[3],recv_bf[4],
          recv_bf[5],recv_bf[6],recv_bf[7],recv_bf[8],recv_bf[9]);
      free(recv_bf);
    } else {
      MPI_Gather(send_bf,10,MPI_DOUBLE,NULL,2,MPI_LONG,0,MPI_COMM_WORLD);
    }
  }



  clock_gettime(CLOCK_MONOTONIC, &time_start);
  MPI_Finalize();
  clock_gettime(CLOCK_MONOTONIC, &time_end);
  tlog_timespec_sub(&time_end,&time_start,&time_diff);
  printf("# MPI_Finalize[%i]: %li.%li\n",world_rank,time_diff.tv_sec,time_diff.tv_nsec);
  clock_gettime(CLOCK_MONOTONIC, &time_gl_end);
  tlog_timespec_sub(&time_gl_end,&time_gl_start,&time_gl_diff);
  printf("# Total run time [%i]: %li.%li\n",world_rank,time_gl_diff.tv_sec,time_gl_diff.tv_nsec);
  exit(EXIT_SUCCESS);
}
