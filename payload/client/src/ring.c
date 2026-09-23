#include "ring.h"
#include <stdlib.h>
#include <string.h>

int mrca_ring_init(struct mrca_ring *r, size_t cap) {
    memset(r, 0, sizeof *r);
    r->buf = malloc(cap);
    if (!r->buf) return -1;
    r->cap = cap;
    pthread_mutex_init(&r->lock, NULL);
    return 0;
}
void mrca_ring_free(struct mrca_ring *r) {
    if (!r) return;
    pthread_mutex_destroy(&r->lock);
    free(r->buf);
    r->buf = NULL; r->cap = 0;
}
size_t mrca_ring_avail(struct mrca_ring *r) {
    pthread_mutex_lock(&r->lock);
    size_t a = r->head - r->tail;
    pthread_mutex_unlock(&r->lock);
    return a;
}
size_t mrca_ring_write(struct mrca_ring *r, const void *src, size_t n) {
    const uint8_t *p = (const uint8_t *)src;
    size_t written = 0;
    pthread_mutex_lock(&r->lock);
    while (written < n) {
        size_t used = r->head - r->tail;
        if (used >= r->cap) { r->dropped += (n - written); break; }
        size_t space = r->cap - used;
        size_t chunk = (n - written < space) ? (n - written) : space;
        size_t idx = r->head % r->cap;
        size_t tailroom = r->cap - idx;
        size_t first = (chunk < tailroom) ? chunk : tailroom;
        memcpy(r->buf + idx, p + written, first);
        if (chunk > first) memcpy(r->buf, p + written + first, chunk - first);
        r->head += chunk;
        written += chunk;
    }
    pthread_mutex_unlock(&r->lock);
    return written;
}
size_t mrca_ring_read(struct mrca_ring *r, void *dst, size_t n) {
    uint8_t *p = (uint8_t *)dst;
    size_t got = 0;
    pthread_mutex_lock(&r->lock);
    size_t used = r->head - r->tail;
    if (n > used) n = used;
    while (got < n) {
        size_t idx = r->tail % r->cap;
        size_t tailroom = r->cap - idx;
        size_t chunk = (n - got < tailroom) ? (n - got) : tailroom;
        memcpy(p + got, r->buf + idx, chunk);
        r->tail += chunk;
        got += chunk;
    }
    pthread_mutex_unlock(&r->lock);
    return got;
}
size_t mrca_ring_peek(struct mrca_ring *r, void *dst, size_t n) {
    size_t saved;
    pthread_mutex_lock(&r->lock);
    size_t used = r->head - r->tail;
    if (n > used) n = used;
    size_t idx = r->tail % r->cap;
    size_t got = 0;
    uint8_t *p = (uint8_t *)dst;
    while (got < n) {
        size_t tailroom = r->cap - idx;
        size_t chunk = (n - got < tailroom) ? (n - got) : tailroom;
        memcpy(p + got, r->buf + idx, chunk);
        idx += chunk; got += chunk;
    }
    saved = r->tail;
    pthread_mutex_unlock(&r->lock);
    (void)saved;
    return got;
}
void mrca_ring_reset(struct mrca_ring *r) {
    pthread_mutex_lock(&r->lock);
    r->head = r->tail = 0;
    pthread_mutex_unlock(&r->lock);
}
