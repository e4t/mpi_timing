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
#define MAX_STR_SIZE 256;

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

#include "mpi_tests.h"

int world_rank = 0;
int world_size = 0;

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

enum run_mode {
  round_trip,
  round_trip_msg_size,
  round_trip_wait,
  round_trip_sync,
  send,
};

struct settings {
  unsigned int nr_runs;
  unsigned fill_random;
  unsigned wait;
  unsigned time_evolution;
  enum run_mode mode;
};

void usage(struct settings mysettings) {
  printf("\tUsage: mpi_init [-rh] MODE\n");
  printf("\tperform small MPI timing test\n");
  printf("\t-h print this help\n");
  printf("\t-r initialize data with (pseudo) random values\n");
  printf("\t-s SEED set random seed\n");
  printf("\t-t TIMES how many times to run the test, default is %i\n",mysettings.nr_runs);
  printf("\t-w MSEC to wait/delay after every round trip, default is %i\n",mysettings.wait);
  printf("\t-e print time evolution instead of min max mean media rms\n");
  printf("\tMODE can be 'round_trip', 'round_trip_msg_size', 'round_trip_wait' ,\
      \n\t'round_trip_sync', 'send'\n");
  printf("\n");
  exit(EXIT_SUCCESS);
}

struct settings parse_cmdline(int argc,char** argv) {
  struct settings mysettings;
  mysettings.nr_runs = 1000;
  mysettings.fill_random = 0;
  mysettings.mode = round_trip;
  mysettings.wait = 20;
  mysettings.time_evolution = 0;
  int opt = 0;
  srand(42);
  while((opt = getopt(argc,argv,"rhs:t:w:e")) != -1 ) {
    switch(opt) {
      case 'r':
        mysettings.fill_random = 1;
        break;
      case 'h':
        usage(mysettings);
        break;
      case 's':
        srand(atoi(optarg));
        break;
      case 't':
        mysettings.nr_runs = (atoi(optarg));
        break;
      case 'w':
        mysettings.wait = (atoi(optarg));
        break;
      case 'e':
        mysettings.time_evolution = 1;
        break;
    }
  }
  for(; optind < argc; optind++){ //when some extra arguments are passed
    if (strcmp("round_trip",argv[optind]) == 0) 
      mysettings.mode = round_trip;
    else if (strcmp("round_trip_msg_size",argv[optind]) == 0) 
      mysettings.mode = round_trip_msg_size;
    else if (strcmp("round_trip_sync",argv[optind]) == 0) 
      mysettings.mode = round_trip_sync;
    else if (strcmp("round_trip_wait",argv[optind]) == 0) 
      mysettings.mode = round_trip_wait;
    else if (strcmp("send",argv[optind]) == 0) 
      mysettings.mode = send;
    else
      usage(mysettings);
  }
  return mysettings;
}

int main(int argc, char** argv) {
  struct timespec time_start, time_end, time_diff, time_gl_start, time_gl_end,time_gl_diff; 
  struct settings mysettings = parse_cmdline(argc,argv);

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

  unsigned int pkg_size = 2, msg_count = 0;
  for(unsigned int i = 4; i <= 14;) {
    /* not only package size of 2 4 8, but 2 3 4 6 8 ... */
      if(pkg_size <(unsigned int) int_pow(2,i) ) {
        pkg_size = int_pow(2,i);
      } else {
        pkg_size += int_pow(2,i+1); 
        pkg_size /= 2;
        i++;
    }
    double *times_snd = calloc(mysettings.nr_runs,sizeof(double));
    double *times_rcv = calloc(mysettings.nr_runs,sizeof(double));
    double *times_prb = calloc(mysettings.nr_runs,sizeof(double));
    for(unsigned int j = 0; j < mysettings.nr_runs; j++) {
      /* now start with the ring test */
      struct timespec time_rcv, time_snd, time_probe; 
      time_snd.tv_sec = 0; time_snd.tv_nsec = 0;
      time_rcv.tv_sec = 0; time_rcv.tv_nsec = 0;
      time_probe.tv_sec = 0; time_probe.tv_nsec = 0;
      switch(mysettings.mode) {
        case round_trip:
          round_trip_func(pkg_size,&time_snd,&time_rcv,msg_count);
          msg_count++;
          break;
        case round_trip_msg_size:
          round_trip_msg_size_func(pkg_size,&time_snd,&time_rcv,&time_probe,msg_count);
          msg_count++;
          break;
        case round_trip_sync:
          round_trip_sync_func(pkg_size,&time_snd,&time_rcv,msg_count);
          msg_count++;
          break;
        case round_trip_wait:
          round_trip_wait_func(pkg_size,&time_snd,&time_rcv,msg_count,mysettings.wait);
          msg_count++;
          break;
        case send:
          send_func(pkg_size,&time_snd,&time_rcv,msg_count);
          msg_count++;
          break;
        default:
          fprintf(stderr,"Invalid mode selected\n");
          exit(EXIT_FAILURE);

      }
      times_snd[j] = tlog_timespec_to_fp(&time_snd);
      times_rcv[j] = tlog_timespec_to_fp(&time_rcv);
      times_prb[j] = tlog_timespec_to_fp(&time_probe);
    }
    if (mysettings.time_evolution == 0) {
      gsl_sort(times_snd,1,mysettings.nr_runs); gsl_sort(times_rcv,1,mysettings.nr_runs);
      gsl_sort(times_prb,1,mysettings.nr_runs);
      double send_bf[15] = {
          gsl_stats_max(times_snd,1,mysettings.nr_runs),gsl_stats_min(times_snd,1,mysettings.nr_runs),
          gsl_stats_mean(times_snd,1,mysettings.nr_runs),gsl_stats_median_from_sorted_data(times_snd,1,mysettings.nr_runs),
          gsl_stats_variance(times_snd,1,mysettings.nr_runs),
          gsl_stats_max(times_rcv,1,mysettings.nr_runs),gsl_stats_min(times_rcv,1,mysettings.nr_runs),
          gsl_stats_mean(times_rcv,1,mysettings.nr_runs),gsl_stats_median_from_sorted_data(times_rcv,1,mysettings.nr_runs),
          gsl_stats_variance(times_rcv,1,mysettings.nr_runs),
          gsl_stats_max(times_prb,1,mysettings.nr_runs),gsl_stats_min(times_prb,1,mysettings.nr_runs),
          gsl_stats_mean(times_prb,1,mysettings.nr_runs),gsl_stats_median_from_sorted_data(times_prb,1,mysettings.nr_runs),
          gsl_stats_variance(times_prb,1,mysettings.nr_runs) };
      if (world_rank == 0 ) {
        double *recv_bf = calloc(world_size * 15,sizeof(double));
        clock_gettime(CLOCK_MONOTONIC, &time_start);
        MPI_Gather(send_bf,15,MPI_DOUBLE,recv_bf,15,MPI_DOUBLE,0,MPI_COMM_WORLD);
        clock_gettime(CLOCK_MONOTONIC, &time_end);
        tlog_timespec_sub(&time_end,&time_start,&time_diff);
        printf("%i %g\n",world_rank,send_bf[12]);
        printf("# Time for gather %lu.%lu\n",time_diff.tv_sec,time_diff.tv_nsec);
        printf("%i",pkg_size);
        printf(" %g %g %g %g %g",
            gsl_stats_max(&recv_bf[0],15,world_size),
            gsl_stats_min(&recv_bf[1],15,world_size),
            gsl_stats_mean(&recv_bf[2],15,world_size),
            gsl_stats_mean(&recv_bf[3],15,world_size),
            gsl_stats_mean(&recv_bf[4],15,world_size));
        printf(" %g %g %g %g %g",
            gsl_stats_max(&recv_bf[5],15,world_size),
            gsl_stats_min(&recv_bf[6],15,world_size),
            gsl_stats_mean(&recv_bf[7],15,world_size),
            gsl_stats_mean(&recv_bf[8],15,world_size),
            gsl_stats_mean(&recv_bf[9],15,world_size));
        printf(" %g %g %g %g %g",
            gsl_stats_max(&recv_bf[10],15,world_size),
            gsl_stats_min(&recv_bf[11],15,world_size),
            gsl_stats_mean(&recv_bf[12],15,world_size),
            gsl_stats_mean(&recv_bf[13],15,world_size),
            gsl_stats_mean(&recv_bf[14],15,world_size));
        printf(" %lu %lu %lu",
            (gsl_stats_max_index(&recv_bf[2],15,world_size)),
            (gsl_stats_max_index(&recv_bf[7],15,world_size)),
            (gsl_stats_max_index(&recv_bf[11],15,world_size)));
        printf("\n");
        free(recv_bf);
      } else {             
        MPI_Gather(send_bf,15,MPI_DOUBLE,NULL,15,MPI_DOUBLE,0,MPI_COMM_WORLD);
      }
    } else {
      double *send_bf = calloc(mysettings.nr_runs*3,sizeof(double));
      memcpy(send_bf,times_snd,mysettings.nr_runs*sizeof(double));
      memcpy(send_bf+mysettings.nr_runs,times_rcv,mysettings.nr_runs*sizeof(double));
      memcpy(send_bf+2*mysettings.nr_runs,times_prb,mysettings.nr_runs*sizeof(double));
      if(world_rank != 0) {
        MPI_Gather(send_bf,mysettings.nr_runs*3,MPI_DOUBLE,
            NULL,mysettings.nr_runs*3,MPI_DOUBLE,0,MPI_COMM_WORLD);
      } else {
        double *rcv_buffer = calloc(mysettings.nr_runs*3*world_size,sizeof(double)); 
        MPI_Gather(send_bf,mysettings.nr_runs*3,MPI_DOUBLE,
            rcv_buffer,mysettings.nr_runs*3,MPI_DOUBLE,0,MPI_COMM_WORLD);
        for(unsigned int k = 0; k < mysettings.nr_runs; k++) {
          printf("%i",pkg_size);
          for(int l = 0; l < world_size; l++) {
            printf(" %g %g %g",rcv_buffer[k+(3*mysettings.nr_runs*l)],
                rcv_buffer[k+mysettings.nr_runs+(3*mysettings.nr_runs*l)],
                rcv_buffer[k+2*mysettings.nr_runs+(3*mysettings.nr_runs*l)]);
          }
          printf("\n");
        }
        free(rcv_buffer);
      }
      free(send_bf);
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
