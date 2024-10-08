#ifndef CILK_H
#define CILK_H

#define __USE_XOPEN
#include <sys/types.h>

#include <pthread.h>
#include <unistd.h>

struct cilk_thread {
    pthread_t * pthread;
};

void cilk_model(void (* f)(void *), void * arg);

int cilk_spawn(
    struct cilk_thread * thread,
    void * (* start_routine)(void *),
    void * restrict arg
);

int cilk_join(struct cilk_thread thread, void ** ret);

int cilk_rand(void);

void cilk_usleep(useconds_t usec);

#endif
