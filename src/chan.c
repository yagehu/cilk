#include <errno.h>
#include <stdlib.h>

#include "cilk/chan.h"

// Allocates and returns a new channel. The capacity specifies whether the
// channel should be buffered or not. A capacity of 0 will create an unbuffered
// channel. Sets errno and returns NULL if initialization failed.
struct chan * chan_new(void) {
    struct chan * chan = (struct chan *) malloc(sizeof(struct chan));
    if (!chan) {
        errno = ENOMEM;
        return NULL;
    }

    if (pthread_mutex_init(&chan->w_mu, NULL) != 0) {
        free(chan);
        return NULL;
    }

    if (pthread_mutex_init(&chan->r_mu, NULL) != 0) {
        pthread_mutex_destroy(&chan->w_mu);
        free(chan);
        return NULL;
    }

    if (pthread_mutex_init(&chan->m_mu, NULL) != 0) {
        pthread_mutex_destroy(&chan->w_mu);
        pthread_mutex_destroy(&chan->r_mu);
        free(chan);
        return NULL;
    }

    if (pthread_cond_init(&chan->r_cond, NULL) != 0) {
        pthread_mutex_destroy(&chan->m_mu);
        pthread_mutex_destroy(&chan->w_mu);
        pthread_mutex_destroy(&chan->r_mu);
        free(chan);
        return NULL;
    }

    if (pthread_cond_init(&chan->w_cond, NULL) != 0) {
        pthread_mutex_destroy(&chan->m_mu);
        pthread_mutex_destroy(&chan->w_mu);
        pthread_mutex_destroy(&chan->r_mu);
        pthread_cond_destroy(&chan->r_cond);
        free(chan);
        return NULL;
    }

    chan->closed = 0;
    chan->r_waiting = 0;
    chan->w_waiting = 0;
    chan->data = NULL;

    return chan;
}

// Releases the channel resources.
void chan_drop(struct chan * chan) {
    pthread_mutex_destroy(&chan->w_mu);
    pthread_mutex_destroy(&chan->r_mu);
    pthread_mutex_destroy(&chan->m_mu);
    pthread_cond_destroy(&chan->r_cond);
    pthread_cond_destroy(&chan->w_cond);

    free(chan);
}

int chan_send(struct chan * chan, void * data) {
    pthread_mutex_lock(&chan->w_mu);
    pthread_mutex_lock(&chan->m_mu);

    if (chan->closed) {
        pthread_mutex_unlock(&chan->m_mu);
        pthread_mutex_unlock(&chan->w_mu);
        errno = EPIPE;
        return -1;
    }

    chan->data = data;
    chan->w_waiting++;

    if (chan->r_waiting > 0) {
        // Signal waiting reader.
        pthread_cond_signal(&chan->r_cond);
    }

    // Block until reader consumed chan->data.
    pthread_cond_wait(&chan->w_cond, &chan->m_mu);

    pthread_mutex_unlock(&chan->m_mu);
    pthread_mutex_unlock(&chan->w_mu);

    return 0;
}

int chan_recv(struct chan * chan, void ** data) {
    pthread_mutex_lock(&chan->r_mu);
    pthread_mutex_lock(&chan->m_mu);

    while (!chan->closed && !chan->w_waiting) {
        // Block until writer has set chan->data.
        chan->r_waiting++;
        pthread_cond_wait(&chan->r_cond, &chan->m_mu);
        chan->r_waiting--;
    }

    if (chan->closed) {
        pthread_mutex_unlock(&chan->m_mu);
        pthread_mutex_unlock(&chan->r_mu);
        errno = EPIPE;
        return -1;
    }

    if (data) {
        *data = chan->data;
    }

    chan->w_waiting--;

    // Signal waiting writer.
    pthread_cond_signal(&chan->w_cond);

    pthread_mutex_unlock(&chan->m_mu);
    pthread_mutex_unlock(&chan->r_mu);

    return 0;
}
