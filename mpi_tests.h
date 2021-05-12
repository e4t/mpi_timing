#ifndef MPI_TESTS_H
#define MPI_TESTS_H

#define MAGIC_START 232323
#define MAGIC_END   424242
#define MAGIC_ID    123123
#include <time.h>

extern int world_rank;
extern int world_size;

void round_trip_func(const unsigned int msg_size,struct timespec *snd_time, 
    struct timespec *rcv_time,int tag);

void round_trip_sync_func(const unsigned int msg_size,struct timespec *snd_time, 
    struct timespec *rcv_time,int tag);

void round_trip_wait_func(const unsigned int msg_size,struct timespec *snd_time, 
    struct timespec *rcv_time,int tag,unsigned int wait);

void round_trip_msg_size_func(const unsigned int msg_size,struct timespec *snd_time, 
    struct timespec *rcv_time,struct timespec* probe_time,int tag);

void send_func(const unsigned int msg_size,struct timespec *snd_time, 
    struct timespec *rcv_time,int tag);

void round_trip_delayed_func(const unsigned int msg_size,struct timespec *snd_time, 
    struct timespec *rcv_time,int tag,unsigned int delay);

#endif
