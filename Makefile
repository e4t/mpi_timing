CFLAGS +=  -std=gnu99 -ggdb
WARNINGS += -Wall -Wextra
LDFLAGS =
LIBRARIES = -lm -lgsl
INCLUDES += -I./
ifndef MPICC
MPICC=mpicc
endif
GSL_CFLAGS != pkg-config --silence-errors --cflags-only-I gsl
GSLBLAS_CFLAGS != pkg-config --silence-errors --cflags-only-I gslcblas
GSL_LIBS != pkg-config --silence-errors --libs gsl
GSLBLAS_LIBS != pkg-config --silence-errors --libs gslcblas
CFLAGS += $(GSL_CFLAGS) $(GSLBLAS_CFLAGS)
LIBRARIES += $(GSL_LIBS) $(GSLBLAS_LIBS)

all: mpi_timing

mpi_tests.o: mpi_tests.c mpi_tests.h
	$(MPICC) -c -o mpi_tests.o mpi_tests.c $(WARNINGS) $(INCLUDES) $(CFLAGS)

mpi_timing.o: mpi_timing.c
	$(MPICC) -c -o mpi_timing.o mpi_timing.c $(WARNINGS) $(INCLUDES) $(CFLAGS)

timespec.o: tlog/timespec.c $(wildcard tlog/*h)
	$(CC) -c -o timespec.o tlog/timespec.c $(WARNINGS) $(INCLUDES) $(CFLAGS)

mpi_timing: mpi_timing.o timespec.o mpi_tests.o
	echo $(LIBRARIES)
	$(MPICC) -o mpi_timing  mpi_timing.o timespec.o mpi_tests.o $(LDFLAGS) $(LIBRARIES) $(CFLAGS)

.PHONY:

archive:
	@git diff-index --quiet HEAD -- || ( echo "uncomitted changes, aborting"; exit 1)
	@git log > CHANGELOG
	@tar --transform="s,^,mpi_timing/," -cjf mpi_timing.tar.bz2 mpi_timing.c mpi_tests.c mpi_tests.h Makefile CHANGELOG tlog/ && \
		echo "Created mpi_timing.tar.bz2"
	@rm CHANGELOG

clean:
	@rm -fv mpi_timing mpi_timing.o timespec.o mpi_tests.o
