#include "cilk.h"

static void func(void * arg) {}

int main(void) {
    cilk_model(func, NULL);
}
