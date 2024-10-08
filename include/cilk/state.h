#ifndef STATE_H
#define STATE_H

#include "cilk/queued_spawn.h"

struct state {
    struct queued_spawn_vec_deque queued_spawn;
};

#endif
