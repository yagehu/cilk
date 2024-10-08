#ifndef REF_H
#define REF_H

#include <stdatomic.h>
#include <stddef.h>

struct ref {
    void (* free)(const struct ref *);
    int  count;
};

#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

static inline void ref_inc(const struct ref * ref) {
    atomic_fetch_add((int *) &ref->count, 1);
}

static inline void ref_dec(const struct ref * ref) {
    if (atomic_fetch_sub((int *) &ref->count, 1) == 1)
        ref->free(ref);
}

#endif
