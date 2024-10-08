#ifndef CONTEXT_H
#define CONTEXT_H

#include <pthread.h>

#include "cilk/ref.h"

enum context_state {
    RUNNING,
    PAUSED,
};

struct shared_context {
    enum context_state state;
    pthread_mutex_t    mu;
    pthread_cond_t     cond;
    struct ref         ref;
};

struct shared_context * shared_context_new(void);

void shared_context_pause(struct shared_context *);

#endif
