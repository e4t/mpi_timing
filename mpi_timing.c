#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <math.h>
#include <mpi.h>


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

int main(int argc, char** argv) {
  struct timespec time_start, time_end; 

  clock_gettime(CLOCK_MONOTONIC, &time_start);
  MPI_Init(&argc,&argv);
  clock_gettime(CLOCK_MONOTONIC, &time_end);
  // Get the number of processes
  int world_size;
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);

  int world_rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

  if (world_rank == 0 ) {
    int mpi_version_len = 0;
    char mpi_version[MPI_MAX_LIBRARY_VERSION_STRING];
    MPI_Get_library_version(mpi_version,&mpi_version_len);
    printf("# MPI version: %s\n",mpi_version);
    printf("# Nr of processors are: %i\n",world_size);
  }


  printf("MPI_Init: %li.%li\n",diff(time_start,time_end).tv_sec,diff(time_start,time_end).tv_nsec);


  // Get the name of the processor
  char processor_name[MPI_MAX_PROCESSOR_NAME];
  int name_len;
  MPI_Get_processor_name(processor_name, &name_len);

  // Print off a hello world message
  printf("Hello world from processor %s, rank %d out of %d processors\n",
         processor_name, world_rank, world_size);

  clock_gettime(CLOCK_MONOTONIC, &time_start);
  MPI_Finalize();
  clock_gettime(CLOCK_MONOTONIC, &time_end);
  printf("MPI_Finalize: %li.%li\n",diff(time_start,time_end).tv_sec,diff(time_start,time_end).tv_nsec);
  return 0;
}
