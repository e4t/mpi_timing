# Small set of MPI tests
Different MPI libraries can use different algorithms for specific MPI functions like `MPI_Allreduce`.

Start e.g. with
  FABRIC="-genv I_MPI_FABRICS=shm:tcp" mpirun -ppn 1 -n 2 -hostfile hostfile2 ./mpi_timing/mpi_timing -t 2000 > mpi_timing.dat
