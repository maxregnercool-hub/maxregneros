#ifndef MRCA_METRICS_H
#define MRCA_METRICS_H
#include "mrca.h"
#include <stdint.h>
#include <stddef.h>

struct mrca_metrics {
    uint64_t frames_rx;
    uint64_t bytes_rx;
    uint64_t events_tx;
    uint64_t frames_dropped;
    uint64_t blits;
    uint64_t blit_ns;
    uint64_t connect_count;
    uint64_t reconnect_count;
    int64_t  last_frame_ms;
    int64_t  last_rtt_ms;
    int64_t  session_start_ms;
    double   fps_ema;
    double   mbps_ema;
};

void metrics_reset(struct mrca_metrics *m);
void metrics_note_frame(struct mrca_metrics *m, size_t bytes, int64_t lat_ms);
void metrics_note_event(struct mrca_metrics *m);
void metrics_note_blit(struct mrca_metrics *m, uint64_t ns);
void metrics_tick(struct mrca_metrics *m);
void metrics_to_json(const struct mrca_metrics *m, char *out, size_t cap);

#endif
