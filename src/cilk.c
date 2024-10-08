#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cilk.h"

struct queued_spawn {
    void * (* f) (void *);
    void * arg;
};

static void queued_spawn_init(struct queued_spawn *, void * (* f)(void *), void * arg);
static void queued_spawn_drop(struct queued_spawn *);

struct queued_spawn_vec {
    size_t len;
    size_t cap;
    struct queued_spawn * items;
};

static void queued_spawn_vec_init(struct queued_spawn_vec *);
static void queued_spawn_vec_drop(struct queued_spawn_vec *);
static void queued_spawn_vec_grow(struct queued_spawn_vec *);
static void queued_spawn_vec_push(struct queued_spawn_vec *, struct queued_spawn);
static bool queued_spawn_vec_pop(struct queued_spawn_vec *, struct queued_spawn *);

struct decision_point {
    size_t num_choices;
};

static void decision_point_init(struct decision_point *);
static void decision_point_drop(struct decision_point *);

struct decision_point_vec {
    size_t len;
    size_t cap;
    struct decision_point * items;
};

static void decision_point_vec_init(struct decision_point_vec *);
static void decision_point_vec_drop(struct decision_point_vec *);
static void decision_point_vec_grow(struct decision_point_vec *);
static void decision_point_vec_push(struct decision_point_vec *, struct decision_point);

struct execution {
    struct decision_point_vec decision_points;
};

static void execution_init(struct execution *);
static void execution_drop(struct execution *);

struct execution_vec {
    size_t len;
    size_t cap;
    struct execution * items;
};

static void execution_vec_init(struct execution_vec *);
static void execution_vec_drop(struct execution_vec *);

static void execution_vec_grow(struct execution_vec *);
static void execution_vec_push(struct execution_vec *, struct execution);

enum thread_state {
    THREAD_STATE_RUNNING,
    THREAD_STATE_PAUSED,
    THREAD_STATE_TERMINATED,
    THREAD_STATE_WAITING,
    THREAD_STATE_JOINING
};

struct thread {
    size_t id;
    enum thread_state * state;
    size_t parent;

    pthread_mutex_t * pause_mu;
    pthread_cond_t * pause_cond;

    pthread_mutex_t * resume_mu;
    pthread_cond_t * resume_cond;
};

static void thread_drop(struct thread *);

struct thread_vec {
    size_t len;
    size_t cap;
    struct thread * items;
};

static void thread_vec_init(struct thread_vec *);
static void thread_vec_drop(struct thread_vec *);

static void thread_vec_grow(struct thread_vec *);
static void thread_vec_push(struct thread_vec *, struct thread);

struct thread_context {
    enum thread_state * state;

    pthread_mutex_t * pause_mu;
    pthread_cond_t * pause_cond;

    pthread_mutex_t * resume_mu;
    pthread_cond_t * resume_cond;
};

static __thread struct thread_context * CTX = NULL;
static __thread pthread_once_t CTX_INIT_ONCE = PTHREAD_ONCE_INIT;

static void thread_context_init_once(void);
static struct thread_context * thread_context(void);

static void thread_context_init(struct thread_context *);
static void thread_context_drop(struct thread_context *);

struct scheduler {
    struct thread_vec threads;
    struct execution * execution;
    struct execution_vec executions;

    /// `cilk_spawn()` queues up the spawns here.
    struct queued_spawn_vec queued_spawns;
    size_t queued_spawn_batch_size;
    atomic_int queued_spawn_batch_count;

    pthread_t * pthreads;
    size_t pthreads_len;

    bool * wakeup;
    pthread_cond_t wakeup_cond;
    pthread_mutex_t wakeup_mu;
};

static struct scheduler * SCHEDULER = NULL;
static pthread_mutex_t SCHEDULER_MU = PTHREAD_MUTEX_INITIALIZER;

static void scheduler_init(struct scheduler *);
static void scheduler_drop(struct scheduler *);

static bool scheduler_is_exhausted(struct scheduler);

static void * run_scheduler(void *);

static void execute(void (* f)(void *), void * arg);

static void cilk_pause(void);
static void cilk_wait(void);

struct run_cilk_thread_params {
    void * (* f)(void *);
    void * arg;
    size_t parent;
};

static void * run_cilk_thread(void * arg);

void cilk_model(void (* f)(void *), void * arg) {
    SCHEDULER = malloc(sizeof(struct scheduler));
    scheduler_init(SCHEDULER);

    pthread_t scheduler_pthread;

    int err = pthread_create(&scheduler_pthread, NULL, run_scheduler, NULL);
    if (err) {
        fprintf(stderr, "[cilk] Failed to spawn scheduler thread.\n");
        exit(1);
    }

    execute(f, arg);

    for (size_t i = 0; i < SCHEDULER->pthreads_len; i++) {
        pthread_join(SCHEDULER->pthreads[i], NULL);
    }

    pthread_join(scheduler_pthread, NULL);
    scheduler_drop(SCHEDULER);
    free(SCHEDULER);
}

int cilk_spawn(
    struct cilk_thread * thread,
    void * (* start_routine)(void *),
    void * arg
) {
    cilk_pause();

    struct queued_spawn spawn;
    queued_spawn_init(&spawn, start_routine, arg);
    queued_spawn_vec_push(&SCHEDULER->queued_spawns, spawn);

    return 0;
}

int cilk_join(struct cilk_thread thread, void ** ret) {
    cilk_wait();

    return 0;
}

void cilk_usleep(useconds_t usec) {
    cilk_pause();
}

static void queued_spawn_init(struct queued_spawn * self, void * (* f)(void *), void * arg) {
    *self = (struct queued_spawn) {
        .f = f,
        .arg = arg,
    };
}

static void queued_spawn_drop(struct queued_spawn * self) {
}

static void queued_spawn_vec_init(struct queued_spawn_vec * self) {
    self->len = 0;
    self->cap = 0;
    self->items = NULL;
}

static void queued_spawn_vec_drop(struct queued_spawn_vec * self) {
    for (size_t i = 0; i < self->len; i++) 
        queued_spawn_drop(&self->items[i]);

    free(self->items);
}

static void queued_spawn_vec_grow(struct queued_spawn_vec * self) {
    self->cap *= 2;

    if (self->cap == 0) self->cap = 1;

    self->items = realloc(self->items, self->cap * sizeof(struct queued_spawn));
}

static void queued_spawn_vec_push(struct queued_spawn_vec * self, struct queued_spawn item) {
    if (self->len == self->cap)
        queued_spawn_vec_grow(self);

    self->items[self->len] = item;
    self->len += 1;
}

static bool queued_spawn_vec_pop(struct queued_spawn_vec * self, struct queued_spawn * item) {
    if (self->len == 0) return false;

    *item = self->items[self->len - 1];
    self->len -= 1;

    return true;
}

static void decision_point_init(struct decision_point * self) {}
static void decision_point_drop(struct decision_point * self) {}

static void decision_point_vec_init(struct decision_point_vec * self) {
    self->len = 0;
    self->cap = 0;
    self->items = NULL;
}

static void decision_point_vec_drop(struct decision_point_vec * self) {
    for (size_t i = 0; i < self->len; i++) {
        decision_point_drop(&self->items[i]);
    }

    free(self->items);
}

static void decision_point_vec_grow(struct decision_point_vec * self) {
    self->cap *= 2;

    if (self->cap == 0) self->cap = 1;

    self->items = realloc(self->items, self->cap * sizeof(struct decision_point));
}

static void decision_point_vec_push(struct decision_point_vec * self, struct decision_point item) {
    if (self->len == self->cap)
        decision_point_vec_grow(self);

    self->items[self->len] = item;
    self->len += 1;
}

static void execution_init(struct execution * self) {
    decision_point_vec_init(&self->decision_points);
}

static void execution_drop(struct execution * self) {
    decision_point_vec_drop(&self->decision_points);
}

static void execution_vec_init(struct execution_vec * self) {
    self->len = 0;
    self->cap = 0;
    self->items = NULL;
}

static void execution_vec_drop(struct execution_vec * self) {
    for (size_t i = 0; i < self->len; i++) {
        execution_drop(&self->items[i]);
    }

    free(self->items);
}

static void execution_vec_grow(struct execution_vec * self) {
    self->cap *= 2;

    if (self->cap == 0) self->cap = 1;

    self->items = realloc(self->items, self->cap * sizeof(struct execution));
}

static void execution_vec_push(struct execution_vec * self, struct execution item) {
    if (self->len == self->cap) {
        execution_vec_grow(self);
    }

    self->items[self->len] = item;
    self->len += 1;
}

static void thread_drop(struct thread * self) {
}

static void thread_vec_init(struct thread_vec * self) {
    self->len = 0;
    self->cap = 0;
    self->items = NULL;
}

static void thread_vec_drop(struct thread_vec * self) {
    for (size_t i = 0; i < self->len; i++)
        thread_drop(&self->items[i]);

    free(self->items);
}

static void thread_vec_grow(struct thread_vec * self) {
    self->cap *= 2;

    if (self->cap == 0) self->cap = 1;

    self->items = realloc(self->items, self->cap * sizeof(struct thread));
}

static void thread_vec_push(struct thread_vec * self, struct thread item) {
    if (self->len == self->cap) {
        thread_vec_grow(self);
    }

    self->items[self->len] = item;
    self->len += 1;
}

static void thread_context_init_once(void) {
    if (CTX != NULL) {
        return;
    }

    CTX = malloc(sizeof(struct thread_context));
    thread_context_init(CTX);
}

static struct thread_context * thread_context(void) {
    pthread_once(&CTX_INIT_ONCE, thread_context_init_once);

    return CTX;
}

static void thread_context_init(struct thread_context * self) {
    self->state = malloc(sizeof(enum thread_state));
    *self->state = THREAD_STATE_RUNNING;

    self->pause_mu = malloc(sizeof(pthread_mutex_t));
    self->pause_cond = malloc(sizeof(pthread_cond_t));

    pthread_mutex_init(self->pause_mu, NULL);
    pthread_cond_init(self->pause_cond, NULL);

    self->resume_mu = malloc(sizeof(pthread_mutex_t));
    self->resume_cond = malloc(sizeof(pthread_cond_t));

    pthread_mutex_init(self->resume_mu, NULL);
    pthread_cond_init(self->resume_cond, NULL);
}

static void thread_context_drop(struct thread_context * self) {
    pthread_mutex_destroy(self->pause_mu);
    pthread_cond_destroy(self->pause_cond);

    pthread_mutex_destroy(self->resume_mu);
    pthread_cond_destroy(self->resume_cond);

    free(self->state);
}

static void scheduler_init(struct scheduler * self) {
    thread_vec_init(&self->threads);
    self->execution = NULL;
    execution_vec_init(&self->executions);
    queued_spawn_vec_init(&self->queued_spawns);

    self->pthreads = NULL;
    self->wakeup = malloc(sizeof(bool));
    *self->wakeup = false;

    pthread_cond_init(&self->wakeup_cond, NULL);
    pthread_mutex_init(&self->wakeup_mu, NULL);
}

static void scheduler_drop(struct scheduler * self) {
    pthread_cond_destroy(&self->wakeup_cond);
    pthread_mutex_destroy(&self->wakeup_mu);
    free(self->wakeup);
    free(self->pthreads);

    queued_spawn_vec_drop(&self->queued_spawns);
    execution_vec_drop(&self->executions);
    thread_vec_drop(&self->threads);
}

static void scheduler_execution_start(struct scheduler * self) {
    self->execution = malloc(sizeof(struct execution));
    execution_init(self->execution);
}

static void scheduler_execution_stop(struct scheduler * self) {
    execution_vec_push(&self->executions, *self->execution);
    free(self->execution);
    self->execution = NULL;
}

static bool scheduler_is_exhausted(struct scheduler self) {
    return true;
}

static void * run_scheduler(void * arg) {
    while (true) {
        pthread_mutex_lock(&SCHEDULER->wakeup_mu);
        while (!*SCHEDULER->wakeup)
            pthread_cond_wait(&SCHEDULER->wakeup_cond, &SCHEDULER->wakeup_mu);
        *SCHEDULER->wakeup = false;
        pthread_mutex_unlock(&SCHEDULER->wakeup_mu);

        pthread_mutex_lock(&SCHEDULER_MU);
        struct thread_vec threads = SCHEDULER->threads;
        pthread_mutex_unlock(&SCHEDULER_MU);

        for (size_t i = 0; i < threads.len; i++) {
            struct thread * thread = &threads.items[i];

            pthread_mutex_lock(thread->pause_mu);
            while (*thread->state == THREAD_STATE_RUNNING) {
                pthread_cond_wait(thread->pause_cond, thread->pause_mu);
            }
            pthread_mutex_unlock(thread->pause_mu);
        }

        // Now all threads are not running.
        bool all_threads_terminated = true;
        for (size_t i = 0; i < threads.len; i++) {
            if (*threads.items[i].state != THREAD_STATE_TERMINATED) {
                all_threads_terminated = false;
                break;
            }
        }

        if (all_threads_terminated) break;

        bool new_spawns = false;

        // Check if any parent thread is waiting (joining).
        // If so, we need to spawn the threads.
        for (size_t i = 0; i < threads.len; i++) {
            struct thread * t = &threads.items[i];

            if (*t->state == THREAD_STATE_WAITING) {
                struct queued_spawn spawn;

                SCHEDULER->queued_spawn_batch_size = SCHEDULER->queued_spawns.len;
                SCHEDULER->queued_spawn_batch_count = 0;

                while (queued_spawn_vec_pop(&SCHEDULER->queued_spawns, &spawn)) {
                    // Dispatch the queued spawn.

                    fprintf(stderr, "[cilk] Spawning queued thread...\n");

                    struct run_cilk_thread_params * params = malloc(sizeof(struct run_cilk_thread_params));

                    *params = (struct run_cilk_thread_params) {
                        .f = spawn.f,
                        .arg = spawn.arg,
                        .parent = t->id,
                    };

                    pthread_t pthread;

                    int err = pthread_create(&pthread, NULL, run_cilk_thread, params);
                    if (err) {
                        fprintf(stderr, "[cilk] Failed to spawn queued thread.\n");
                        exit(1);
                    }

                    SCHEDULER->pthreads_len += 1;
                    SCHEDULER->pthreads = realloc(SCHEDULER->pthreads, SCHEDULER->pthreads_len * sizeof(pthread_t));
                    SCHEDULER->pthreads[SCHEDULER->pthreads_len - 1] = pthread;

                    new_spawns = true;
                }

                while (SCHEDULER->queued_spawn_batch_count != SCHEDULER->queued_spawn_batch_size);

                // Resume the waiting parent thread.
                pthread_mutex_lock(t->resume_mu);
                *t->state = THREAD_STATE_JOINING;
                pthread_cond_signal(t->resume_cond);
                pthread_mutex_unlock(t->resume_mu);
            }
        }

        if (new_spawns) continue;

        struct thread_vec candidates;
        thread_vec_init(&candidates);

        for (size_t i = 0; i < threads.len; i++) {
            struct thread * t = &threads.items[i];

            if (*t->state == THREAD_STATE_PAUSED) {
                thread_vec_push(&candidates, *t);
            }
        }

        if (candidates.len == 0) continue;

        size_t choice = rand() % candidates.len;

        fprintf(stderr, "[cilk] Decision point with %zu choice(s). Picking idx %zu.\n", candidates.len, choice);
        decision_point_vec_push(&SCHEDULER->execution->decision_points, (struct decision_point){
            .num_choices = SCHEDULER->threads.len,
        });


        // Unpause a thread.

        struct thread * thread = &candidates.items[choice];

        pthread_mutex_lock(thread->resume_mu);
        *thread->state = THREAD_STATE_RUNNING;
        pthread_cond_signal(thread->resume_cond);
        pthread_mutex_unlock(thread->resume_mu);

        thread_vec_drop(&candidates);
    }

    return NULL;
}

static void execute(void (* f)(void *), void * arg) {
    struct thread_context * ctx = thread_context();

    pthread_mutex_lock(&SCHEDULER_MU);
    scheduler_execution_start(SCHEDULER);
    thread_vec_push(&SCHEDULER->threads, (struct thread) {
        .id = 0,
        .state = ctx->state,
        .pause_mu = ctx->pause_mu,
        .pause_cond = ctx->pause_cond,
        .resume_cond = ctx->resume_cond,
        .resume_mu = ctx->resume_mu,
    });
    pthread_mutex_unlock(&SCHEDULER_MU);

    (f)(arg);

    *SCHEDULER->threads.items[0].state = THREAD_STATE_TERMINATED;

    pthread_mutex_lock(&SCHEDULER->wakeup_mu);
    *SCHEDULER->wakeup = true;
    pthread_cond_signal(&SCHEDULER->wakeup_cond);
    pthread_mutex_unlock(&SCHEDULER->wakeup_mu);
}

static inline void cilk_pause(void) {
    struct thread_context * ctx = thread_context();

    pthread_mutex_lock(ctx->pause_mu);
    assert(*ctx->state == THREAD_STATE_RUNNING);
    *ctx->state = THREAD_STATE_PAUSED;
    pthread_cond_signal(ctx->pause_cond);
    pthread_mutex_unlock(ctx->pause_mu);

    pthread_mutex_lock(&SCHEDULER->wakeup_mu);
    *SCHEDULER->wakeup = true;
    pthread_cond_signal(&SCHEDULER->wakeup_cond);
    pthread_mutex_unlock(&SCHEDULER->wakeup_mu);

    pthread_mutex_lock(ctx->resume_mu);
    while (*ctx->state == THREAD_STATE_PAUSED) {
        pthread_cond_wait(ctx->resume_cond, ctx->resume_mu);
    }
    assert(*ctx->state == THREAD_STATE_RUNNING);
    pthread_mutex_unlock(ctx->resume_mu);
}

static inline void cilk_wait(void) {
    struct thread_context * ctx = thread_context();

    pthread_mutex_lock(ctx->pause_mu);
    assert(*ctx->state == THREAD_STATE_RUNNING);
    *ctx->state = THREAD_STATE_WAITING;
    pthread_cond_signal(ctx->pause_cond);
    pthread_mutex_unlock(ctx->pause_mu);

    pthread_mutex_lock(&SCHEDULER->wakeup_mu);
    *SCHEDULER->wakeup = true;
    pthread_cond_signal(&SCHEDULER->wakeup_cond);
    pthread_mutex_unlock(&SCHEDULER->wakeup_mu);

    pthread_mutex_lock(ctx->resume_mu);
    while (*ctx->state == THREAD_STATE_WAITING) {
        pthread_cond_wait(ctx->resume_cond, ctx->resume_mu);
    }
    assert(*ctx->state == THREAD_STATE_JOINING);
    pthread_mutex_unlock(ctx->resume_mu);

    // Now the thread needs to wait for the child threads to finish.
    for (size_t i = 0; i < SCHEDULER->threads.len; i++) {
        struct thread * t = &SCHEDULER->threads.items[i];

        if (t->state == ctx->state) continue;

        while (*t->state != THREAD_STATE_TERMINATED) {
        }
    }

    *ctx->state = THREAD_STATE_RUNNING;
}

static void * run_cilk_thread(void * arg) {
    struct run_cilk_thread_params * params = arg;
    struct thread_context * ctx = thread_context();

    pthread_mutex_lock(&SCHEDULER_MU);
    thread_vec_push(&SCHEDULER->threads, (struct thread) {
        .id = SCHEDULER->threads.len,
        .state = ctx->state,
        .parent = params->parent,
        .pause_mu = ctx->pause_mu,
        .pause_cond = ctx->pause_cond,
        .resume_mu = ctx->resume_mu,
        .resume_cond = ctx->resume_cond,
    });
    SCHEDULER->queued_spawn_batch_count += 1;
    pthread_mutex_unlock(&SCHEDULER_MU);

    // Pause
    cilk_pause();

    void * ret = (params->f)(params->arg);

    pthread_mutex_lock(&SCHEDULER->wakeup_mu);
    *SCHEDULER->wakeup = true;
    pthread_cond_signal(&SCHEDULER->wakeup_cond);
    pthread_mutex_unlock(&SCHEDULER->wakeup_mu);

    *ctx->state = THREAD_STATE_TERMINATED;

    return ret;
}
