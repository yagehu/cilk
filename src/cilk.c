#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cilk.h"
#include "cilk/context.h"
#include "cilk/scheduler.h"
#include "cilk/state.h"

static __thread struct shared_context * ctx = NULL;
static pthread_once_t ctx_init_once = PTHREAD_ONCE_INIT;

static __thread struct scheduler * SCHEDULER = NULL;

static struct scheduler * scheduler(void) {
    if (SCHEDULER == NULL) {
        fprintf(stderr, "[cilk] Model not started.\n");
        exit(1);
    }

    return SCHEDULER;
}

static void scheduler_global_init(void) {
    SCHEDULER = malloc(sizeof(struct scheduler));
    scheduler_init(SCHEDULER);
}

static void scheduler_global_drop(void) {
    scheduler_drop(SCHEDULER);
    free(SCHEDULER);
    SCHEDULER = NULL;
}

void cilk_start(void) {
    if (SCHEDULER != NULL) {
        fprintf(stderr, "[cilk] Model already started.\n");
        cilk_stop();
        exit(1);
    }

     scheduler_global_init();
}

void cilk_stop(void) {
    scheduler_global_drop();
}

static inline void shared_context_init(void) {
    ctx = shared_context_new();
}

static inline void cilk_pause(void) {
    shared_context_pause(ctx);
}

struct managed_thread_params {
    void * (* start_routine)(void *);
    void * arg;
};

static void * managed_thread_run(void * arg) {
    pthread_once(&ctx_init_once, shared_context_init);

    struct managed_thread_params * params = arg;

    (params->start_routine)(params->arg);

    free(params);
}

int cilk_managed_thread_create(
    struct cilk_managed_thread * thread,
    const pthread_attr_t * restrict attr,
    void * (* start_routine)(void *),
    void * restrict arg
) {
    struct scheduler * sched = scheduler();

    scheduler_spawn(sched, start_routine, arg);

    return 0;
}

int cilk_managed_thread_join(struct cilk_managed_thread thread, void ** retval) {
    int ret = pthread_join(thread.pthread, retval);

    return ret;
}

int cilk_rand(void) {
    return 0;
}

void cilk_usleep(useconds_t usec) {
    pause();
}
