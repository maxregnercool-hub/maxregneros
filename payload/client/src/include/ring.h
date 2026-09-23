#ifndef MRCA_RING_H
#define MRCA_RING_H
#include "mrca.h"
#include <stddef.h>
#include <stdint.h>
#include <pthread.h>

/* Single-producer/single-consumer lock-light byte ring. Used to decouple the
 * socket reader from the display writer so a slow blit never stalls the wire. */
struct mrca_ring {
    uint8_t        *buf;
    size_t          cap;
    size_t          head;
    size_t          tail;
    pthread_mutex_t lock;
    uint64_t        dropped;
};

int  mrca_ring_init(struct mrca_ring *r, size_t cap);
void mrca_ring_free(struct mrca_ring *r);
size_t mrca_ring_write(struct mrca_ring *r, const void *src, size_t n);
size_t mrca_ring_read(struct mrca_ring *r, void *dst, size_t n);
size_t mrca_ring_peek(struct mrca_ring *r, void *dst, size_t n);
size_t mrca_ring_avail(struct mrca_ring *r);
void mrca_ring_reset(struct mrca_ring *r);

#endif
