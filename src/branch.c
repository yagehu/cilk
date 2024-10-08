#include <stdlib.h>

#include "cilk/branch.h"

void branch_vec_init(struct branch_vec * vec) {
    vec->len = 0;
    vec->cap = 0;
    vec->data = NULL;
}

void branch_vec_drop(struct branch_vec * vec) {
    free(vec->data);
}

static void branch_vec_grow(struct branch_vec * vec) {
    vec->cap *= 2;

    if (vec->cap == 0) {
        vec->cap = 1;
    }

    vec->data = realloc(vec->data, vec->cap * sizeof(struct branch));
}

void branch_vec_push(struct branch_vec * vec, struct branch branch) {
    if (vec->len == vec->cap) {
        branch_vec_grow(vec);
    }

    vec->data[vec->len] = branch;
    vec->len += 1;
}

const struct branch * branch_vec_get(struct branch_vec * vec, unsigned int i) {
    if (i >= vec->len) {
        return NULL;
    }

    return &vec->data[i];
}
