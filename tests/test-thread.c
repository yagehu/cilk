#include <assert.h>
#include <stdio.h>

#include "cilk.h"

static int results[2] = { 0, 0 };

static void * thread_main(void * arg) {
    int idx = * (int *) arg;

    results[idx] = idx + 1;

    return NULL;
}

static void func(void * arg) {
    struct cilk_thread t0;
    struct cilk_thread t1;

    int idx0 = 0;
    int idx1 = 1;

    results[0] = 0;
    results[1] = 0;

    cilk_spawn(&t0, thread_main, &idx0);
    cilk_spawn(&t1, thread_main, &idx1);
    cilk_join(t0, NULL);
    cilk_join(t1, NULL);

    assert(results[0] == 1);
    assert(results[1] == 2);
}

int main(void) {
    cilk_model(func, NULL);
}
