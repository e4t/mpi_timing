# Small set of MPI tests
Different MPI libraries can use different algorithms for specific MPI functions like `MPI_Allreduce`.

Start e.g. with
  FABRIC="-genv I_MPI_FABRICS=shm:tcp" mpirun -ppn 1 -n 2 -hostfile hostfile2 ./mpi_timing/mpi_timing -t 2000 > mpi_timing.dat
or in a loop
  for i in $(seq 1 5); do FABRIC="-genv I_MPI_FABRICS=shm:tcp" mpirun -ppn 1 -n 4 -hostfile hostfile4 ./mpi_timing/mpi_timing -t 8000 -r > mpi_timing.${i}_4.dat; done

