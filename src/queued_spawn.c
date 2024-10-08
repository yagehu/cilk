#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cilk/queued_spawn.h"

void queued_spawn_drop(struct queued_spawn * self) {
}

void queued_spawn_vec_deque_init(struct queued_spawn_vec_deque * self) {
    self->len = 0;
    self->cap = 0;
    self->head = 0;
    self->items = NULL;
}

void queued_spawn_vec_deque_drop(struct queued_spawn_vec_deque * self) {
    size_t head_len = self->cap - self->head;

    if (head_len >= self->len) {
        for (int i = self->head; i < self->head + self->len; i++) {
            queued_spawn_drop(&self->items[i]);
        }
    } else {
        for (int i = self->head; i < self->cap; i++) {
            queued_spawn_drop(&self->items[i]);
        }

        for (int i = 0; i < self->len - head_len; i++) {
            queued_spawn_drop(&self->items[i]);
        }
    }

    free(self->items);
}

static void queued_spawn_vec_deque_grow(struct queued_spawn_vec_deque * self) {
    size_t old_cap = self->cap;

    self->cap *= 2;

    if (self->cap == 0) {
        self->cap = 1;
    }

    fflush(stderr);

    self->items = realloc(self->items, self->cap * sizeof(struct queued_spawn));

    if (self->head + self->len <= old_cap) {
        return;
    }

    size_t head_len = old_cap - self->head;
    size_t tail_len = self->len - head_len;

    if (head_len > tail_len && self->cap - old_cap >= tail_len) {
        // Assumption: non-overlapping
        memcpy(&self->items[old_cap], self->items, tail_len * sizeof(struct queued_spawn));
        return;
    }

    size_t new_head = self->cap - head_len;

    memmove(&self->items[self->head], &self->items[new_head], head_len * sizeof(struct queued_spawn));
    self->head = new_head;
}

static size_t queued_spawn_vec_deque_to_physical_idx(struct queued_spawn_vec_deque * self, size_t i) {
    if (self->head + i >= self->cap) {
        return self->head + i - self->cap;
    } else {
        return self->head + i;
    }
}

void queued_spawn_vec_deque_push_back(struct queued_spawn_vec_deque * self, struct queued_spawn item) {
    if (self->len == self->cap) {
        queued_spawn_vec_deque_grow(self);
    }

    size_t idx = queued_spawn_vec_deque_to_physical_idx(self, self->len);
    self->items[idx] = item;
    self->len += 1;
}
