#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "cilk/scheduler.h"
#include "cilk/queued_spawn.h"

struct state {
    struct queued_spawn_vec_deque * queued_spawn;
};

static __thread struct state * STATE = NULL;
static __thread pthread_once_t STATE_INIT_ONCE = PTHREAD_ONCE_INIT;

static struct state * state(void);

static void state_init(struct state *);
static void state_global_init(void) {
    STATE = malloc(sizeof(struct state));

    state_init(STATE);
}

void scheduler_init(struct scheduler *) {
}

void scheduler_drop(struct scheduler * scheduler) {
}

void scheduler_spawn(struct scheduler * self, void * (* f)(void *), void * arg) {
    queued_spawn_vec_deque_push_back(state()->queued_spawn, (struct queued_spawn) {
        .f = f,
        .arg = arg,
    });
}

static struct state * state(void) {
    pthread_once(&STATE_INIT_ONCE, state_global_init);

    return STATE;
}

static void state_init(struct state * state) {
    state->queued_spawn = malloc(sizeof(struct queued_spawn_vec_deque));
    queued_spawn_vec_deque_init(state->queued_spawn);
}

static void state_drop(struct state * self) {
    queued_spawn_vec_deque_drop(self->queued_spawn);
    free(self->queued_spawn);
}
