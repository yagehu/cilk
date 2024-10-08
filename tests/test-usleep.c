#include <stdio.h>

#include "cilk.h"

static void func(void * arg) {
    fprintf(stderr, "[func] usleep...\n");
    cilk_usleep(1);
}

int main(void) {
    cilk_model(func, NULL);
}
