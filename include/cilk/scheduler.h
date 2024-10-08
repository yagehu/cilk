#ifndef SCHEDULER_H
#define SCHEDULER_H

struct scheduler {

};

void scheduler_init(struct scheduler *);
void scheduler_drop(struct scheduler *);

void scheduler_spawn(struct scheduler *, void * (* start_routine)(void *), void * arg);

#endif
