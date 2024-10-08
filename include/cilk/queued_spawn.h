#ifndef QUEUED_SPAWN_H
#define QUEUED_SPAWN_H

#include <stddef.h>

struct queued_spawn {
    void * (* f)(void *);
    void * arg;
};

void queued_spawn_drop(struct queued_spawn *);

struct queued_spawn_vec_deque {
    size_t len;
    size_t cap;
    size_t head;
    struct queued_spawn * items;
};

void queued_spawn_vec_deque_init(struct queued_spawn_vec_deque *);
void queued_spawn_vec_deque_drop(struct queued_spawn_vec_deque *);

void queued_spawn_vec_deque_push_back(struct queued_spawn_vec_deque *, struct queued_spawn);


#endif
