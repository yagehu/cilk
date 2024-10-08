#include <pthread.h>

// Defines a thread-safe unbuffered communication pipe.
struct chan {
    pthread_mutex_t r_mu;
    pthread_mutex_t w_mu;
    void *          data;

    pthread_mutex_t m_mu;
    pthread_cond_t  r_cond;
    pthread_cond_t  w_cond;
    int             closed;
    int             r_waiting;
    int             w_waiting;
};

struct chan * chan_new(void);
void chan_drop(struct chan * chan);

int chan_send(struct chan * chan, void * data);
int chan_recv(struct chan * chan, void ** data);

