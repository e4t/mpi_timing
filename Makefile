CFLAGS += 
WARNINGS += -Wall -Wextra
LDFLAGS =
LIBRARIES = -lm -lgslcblas -lgsl
INCLUDES += -I./

all: mpi_timing

mpi_timing.o: mpi_timing.c
	mpicc -c -o mpi_timing.o mpi_timing.c $(WARNINGS) $(INCLUDES)

timespec.o: tlog/timespec.c $(wildcard tlog/*h)
	$(CC) -c -o timespec.o tlog/timespec.c $(WARNINGS) $(INCLUDES)

mpi_timing: mpi_timing.o timespec.o
	mpicc -o mpi_timing  mpi_timing.o timespec.o $(LDFLAGS) $(LIBRARIES)

.PHONY:

clean:
	@rm -fv mpi_timing mpi_timing.o timespec.o
