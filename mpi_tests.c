#include "mpi_tests.h"
#include <mpi.h>
#include <assert.h>
#include <stdlib.h>
#include "tlog/timespec.h"

void round_trip_func(const unsigned int msg_size,struct timespec *snd_time, 
    struct timespec *rcv_time,int tag) {
  assert(msg_size >= 3);
  int * data = calloc(msg_size,sizeof(int));
  data[0] = MAGIC_START; data[msg_size-1] = MAGIC_END;
  data[1] = tag;
  int msg_id = MAGIC_ID;
  struct timespec time_start, time_end;
  if(world_rank != 0) {
    clock_gettime(CLOCK_MONOTONIC, &time_start);
    MPI_Recv(data,msg_size,MPI_INT,
        world_rank - 1,msg_id,MPI_COMM_WORLD,MPI_STATUS_IGNORE);
    clock_gettime(CLOCK_MONOTONIC, &time_end);
    tlog_timespec_sub(&time_end,&time_start,rcv_time);
  } else {
    if(tag == -1) {
      for(unsigned int i = 2; i < msg_size-1; i++) {
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

void round_trip_sync_func(const unsigned int msg_size,struct timespec *snd_time, 
    struct timespec *rcv_time,int tag) {
  if(MPI_Barrier(MPI_COMM_WORLD) != MPI_SUCCESS) {
    fprintf(stderr,"Barrier was not successfull on rank %i\n",world_rank);
    MPI_Abort(MPI_COMM_WORLD,EXIT_FAILURE);
    exit(EXIT_FAILURE);
  }
  round_trip_func(msg_size,snd_time,rcv_time,tag);
}

void round_trip_wait_func(const unsigned int msg_size,struct timespec *snd_time, 
    struct timespec *rcv_time,int tag,unsigned int wait) {
  usleep(wait);
  round_trip_func(msg_size,snd_time,rcv_time,tag);
}

void round_trip_msg_size_func(const unsigned int msg_size,struct timespec *snd_time, 
    struct timespec *rcv_time,struct timespec* probe_time,int tag) {
  assert(msg_size >= 3);
  int * data = calloc(msg_size,sizeof(int));
  data[0] = MAGIC_START; data[msg_size-1] = MAGIC_END;
  data[1] = tag;
  int msg_id = MAGIC_ID, msg_size_status = 0;
  MPI_Status status;
  struct timespec time_start, time_end ;
  if(world_rank != 0) {
    clock_gettime(CLOCK_MONOTONIC, &time_start);
    MPI_Probe(world_rank-1,msg_id,MPI_COMM_WORLD,&status);
    clock_gettime(CLOCK_MONOTONIC, &time_end);
    tlog_timespec_sub(&time_end,&time_start,probe_time);

    MPI_Get_count(&status,MPI_INT,&msg_size_status);
    if(msg_size_status != (int) msg_size) {
      fprintf(stderr,"Messages sizes differs on rank %i: %i <-> %i\n",
          world_rank,msg_size_status,msg_size);
      exit(EXIT_FAILURE);
    }
    clock_gettime(CLOCK_MONOTONIC, &time_start);
    MPI_Recv(data,msg_size,MPI_INT,
        world_rank - 1,msg_id,MPI_COMM_WORLD,MPI_STATUS_IGNORE);
    clock_gettime(CLOCK_MONOTONIC, &time_end);
    tlog_timespec_sub(&time_end,&time_start,rcv_time);
  } else {
    if(tag == -1) {
      for(unsigned int i = 2; i < msg_size-1; i++) {
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
    MPI_Probe(world_size-1,msg_id,MPI_COMM_WORLD,&status);
    clock_gettime(CLOCK_MONOTONIC, &time_end);
    tlog_timespec_sub(&time_end,&time_start,probe_time);

    MPI_Get_count(&status,MPI_INT,&msg_size_status);
    if(msg_size_status != (int) msg_size) {
      fprintf(stderr,"Messages sizes differs on rank %i: %i <-> %i\n",
          world_rank,msg_size_status,msg_size);
      exit(EXIT_FAILURE);
    }

    clock_gettime(CLOCK_MONOTONIC, &time_start);
    MPI_Recv(data,msg_size,MPI_INT,
        world_size-1,msg_id,MPI_COMM_WORLD,MPI_STATUS_IGNORE);
    clock_gettime(CLOCK_MONOTONIC, &time_end);
    tlog_timespec_sub(&time_end,&time_start,rcv_time);
  }

  free(data);
}

void send_func(const unsigned int msg_size,struct timespec *snd_time, 
    struct timespec *rcv_time,int tag) {
  assert(world_size >= 2);
  assert(msg_size >= 3);
  int * data = calloc(msg_size,sizeof(int));
  data[0] = MAGIC_START; data[msg_size-1] = MAGIC_END;
  data[1] = tag;
  int msg_id = MAGIC_ID;
  struct timespec time_start, time_end;
  if(world_rank % 2 != 0) {
    clock_gettime(CLOCK_MONOTONIC, &time_start);
    MPI_Recv(data,msg_size,MPI_INT,
        world_rank - 1,msg_id,MPI_COMM_WORLD,MPI_STATUS_IGNORE);
    clock_gettime(CLOCK_MONOTONIC, &time_end);
    tlog_timespec_sub(&time_end,&time_start,rcv_time);
  } else {
    if(tag == -1) {
      for(unsigned int i = 2; i < msg_size-1; i++) {
        data[i] = rand();
      }
    }
    clock_gettime(CLOCK_MONOTONIC, &time_start);
    MPI_Send(data,msg_size,MPI_INT,
        (world_rank + 1) % world_size,msg_id,MPI_COMM_WORLD);
    clock_gettime(CLOCK_MONOTONIC, &time_end);
    tlog_timespec_sub(&time_end,&time_start,snd_time );
  }
  free(data);
}

