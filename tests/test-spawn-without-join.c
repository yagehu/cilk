#include "cilk.h"

static void * func(void *);

int main(void) {
    cilk_start();

    struct cilk_managed_thread thread;

    cilk_managed_thread_create(&thread, NULL, func, NULL);

    cilk_stop();
}

static void * func(void * arg) {
}
