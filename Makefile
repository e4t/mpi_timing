CFLAGS +=  -std=gnu99
WARNINGS += -Wall -Wextra
LDFLAGS =
LIBRARIES = -lm -lgslcblas -lgsl
INCLUDES += -I./
ifndef MPICC
MPICC=mpicc
endif

all: mpi_timing

mpi_timing.o: mpi_timing.c
	$(MPICC) -c -o mpi_timing.o mpi_timing.c $(WARNINGS) $(INCLUDES) $(CFLAGS)

timespec.o: tlog/timespec.c $(wildcard tlog/*h)
	$(CC) -c -o timespec.o tlog/timespec.c $(WARNINGS) $(INCLUDES) $(CFLAGS)

mpi_timing: mpi_timing.o timespec.o
	$(MPICC) -o mpi_timing  mpi_timing.o timespec.o $(LDFLAGS) $(LIBRARIES) $(CFLAGS)

.PHONY:

archive:
	@tar --transform="s,^,mpi_timing/," -cjf mpi_timing.tar.bz2 mpi_timing.c Makefile tlog/ && \
		echo "Created mpi_timing.tar.bz2"

clean:
	@rm -fv mpi_timing mpi_timing.o timespec.o
