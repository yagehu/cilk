#ifndef BRANCH_H
#define BRANCH_H

#include <stddef.h>

enum branch_type {
    BRANCH_TYPE_SCHEDULE,
};

struct branch {
    enum branch_type type;

    union {

    };
};

struct branch_vec {
    size_t          len;
    size_t          cap;
    struct branch * data;
};

void branch_vec_init(struct branch_vec *);

#endif
