#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "cilk/context.h"
#include "cilk/location.h"

void shared_context_free(const struct ref * ref) {
    struct shared_context * ctx = container_of(ref, struct shared_context, ref);

    free(ctx);
}

struct shared_context * shared_context_new(void) {
    struct location loc = LOC;
    struct shared_context * ctx = malloc(sizeof(struct shared_context));

    pthread_mutex_init(&ctx->mu, NULL);
    pthread_cond_init(&ctx->cond, NULL);

    ctx->ref = (struct ref) { shared_context_free, 1 };

    return ctx;
}

void shared_context_pause(struct shared_context * ctx) {
    pthread_mutex_lock(&ctx->mu);

    assert(ctx->state == RUNNING);

    ctx->state = PAUSED;
    pthread_cond_signal(&ctx->cond);

    while (ctx->state == PAUSED) {
        pthread_cond_wait(&ctx->cond, &ctx->mu);
    }

    assert(ctx->state == RUNNING);

    pthread_mutex_unlock(&ctx->mu);
}
