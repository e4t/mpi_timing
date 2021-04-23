#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>

#include <mpi.h>
#include "mpi_timing.h"


static unsigned int fill_random = 0;
static int nr_runs = 10;
static int world_rank = 0;
static int world_size = 0;

struct timespec diff(struct timespec start,struct timespec end) {
    struct timespec temp;
    if ((end.tv_nsec-start.tv_nsec)<0) {
        temp.tv_sec = end.tv_sec-start.tv_sec-1;
        temp.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
    } else {
        temp.tv_sec = end.tv_sec-start.tv_sec;
        temp.tv_nsec = end.tv_nsec-start.tv_nsec;
    }
    return temp;
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
    *rcv_time = diff(time_start,time_end);
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
  *snd_time = diff(time_start,time_end);
  if(world_rank == 0) {
    clock_gettime(CLOCK_MONOTONIC, &time_start);
    MPI_Recv(data,msg_size,MPI_INT,
        world_size-1,msg_id,MPI_COMM_WORLD,MPI_STATUS_IGNORE);
    clock_gettime(CLOCK_MONOTONIC, &time_end);
    *rcv_time = diff(time_start,time_end);
  }

  free(data);
}

int main(int argc, char** argv) {
  struct timespec time_start, time_end; 
  parse_cmdline(&argc,&argv);


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
  send_bf_init[0] = diff(time_start,time_end).tv_sec; send_bf_init[1] = diff(time_start,time_end).tv_nsec;
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
      printf("%i %lu.%lu\n",i,recv_bf_init[2*i],recv_bf_init[2*i+1]);
    }

    free(recv_bf_init);
    free(recv_bf_proc);
  } else {
    MPI_Gather(send_bf_init,2,MPI_LONG,NULL,2,MPI_LONG,0,MPI_COMM_WORLD);
    MPI_Gather(processor_name,MPI_MAX_PROCESSOR_NAME,MPI_CHAR,NULL,MPI_MAX_PROCESSOR_NAME,MPI_CHAR,0,MPI_COMM_WORLD);
  }
  free(send_bf_init);

  for(int i = 0; i < nr_runs; i++) {
    round_trip(256,&time_start,&time_end);
  }


  clock_gettime(CLOCK_MONOTONIC, &time_start);
  MPI_Finalize();
  clock_gettime(CLOCK_MONOTONIC, &time_end);
  printf("# MPI_Finalize[%i]: %li.%li\n",world_rank,diff(time_start,time_end).tv_sec,diff(time_start,time_end).tv_nsec);
  exit(EXIT_SUCCESS);
}
