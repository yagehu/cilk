#ifndef cilk_H
#define cilk_H

#define __USE_XOPEN
#include <sys/types.h>

#include <pthread.h>
#include <unistd.h>

void cilk_start(void);
void cilk_stop(void);

struct cilk_managed_thread {
    pthread_t               pthread;
    struct chan *           tx;
    struct shared_context * ctx;
};

int cilk_managed_thread_create(
    struct cilk_managed_thread * thread,
    const pthread_attr_t * restrict attr,
    void * (* start_routine)(void *),
    void * restrict arg
);

int cilk_managed_thread_join(struct cilk_managed_thread thread, void ** retval);

int cilk_rand(void);

void cilk_usleep(useconds_t usec);

#endif
