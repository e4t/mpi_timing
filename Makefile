CFLAGS += 
WARNINGS += -Werror -Wall -Wextra -pedantic-errors
LDFLAGS =
LIBRARIES =

all: mpi_timing

mpi_timing.o: mpi_timing.c mpi_timing.h
	mpicc -c -o mpi_timing.o mpi_timing.c $(WARNINGS)

mpi_timing: mpi_timing.o
	mpicc -o mpi_timing mpi_timing.o $(LDFLAGS)



.PHONY:

clean:
	@rm -fv mpi_timing mpi_timing.o
