#include "metrics.h"
#include "util.h"
#include <stdio.h>
#include <string.h>

void metrics_reset(struct mrca_metrics *m) {
    memset(m, 0, sizeof *m);
    m->session_start_ms = mrca_now_ms();
}

void metrics_note_frame(struct mrca_metrics *m, size_t bytes, int64_t lat_ms) {
    m->frames_rx++;
    m->bytes_rx += bytes;
    m->last_frame_ms = mrca_now_ms();
    if (lat_ms > 0) m->last_rtt_ms = lat_ms;
}

void metrics_note_event(struct mrca_metrics *m) { m->events_tx++; }

void metrics_note_blit(struct mrca_metrics *m, uint64_t ns) {
    m->blits++;
    m->blit_ns += ns;
}

/* Exponential moving averages, 1/8 weight on the newest sample. */
void metrics_tick(struct mrca_metrics *m) {
    static int64_t last_tick = 0;
    static uint64_t last_frames = 0, last_bytes = 0;
    int64_t now = mrca_now_ms();
    if (!last_tick) { last_tick = now; last_frames = m->frames_rx; last_bytes = m->bytes_rx; return; }
    int64_t dt = now - last_tick;
    if (dt < 500) return;
    double secs = dt / 1000.0;
    double fps = (double)(m->frames_rx - last_frames) / secs;
    double mbps = (double)(m->bytes_rx - last_bytes) * 8.0 / secs / 1e6;
    m->fps_ema  = m->fps_ema  * 0.875 + fps  * 0.125;
    m->mbps_ema = m->mbps_ema * 0.875 + mbps * 0.125;
    last_tick = now; last_frames = m->frames_rx; last_bytes = m->bytes_rx;
}

void metrics_to_json(const struct mrca_metrics *m, char *out, size_t cap) {
    double avg_blit_us = m->blits ? (double)m->blit_ns / m->blits / 1000.0 : 0.0;
    snprintf(out, cap,
        "{\"frames\":%llu,\"bytes\":%llu,\"events\":%llu,\"dropped\":%llu,"
        "\"fps\":%.2f,\"mbps\":%.2f,\"blit_us\":%.1f,\"rtt_ms\":%lld,"
        "\"connects\":%llu,\"reconnects\":%llu,\"uptime_ms\":%lld}",
        (unsigned long long)m->frames_rx,
        (unsigned long long)m->bytes_rx,
        (unsigned long long)m->events_tx,
        (unsigned long long)m->frames_dropped,
        m->fps_ema, m->mbps_ema, avg_blit_us,
        (long long)m->last_rtt_ms,
        (unsigned long long)m->connect_count,
        (unsigned long long)m->reconnect_count,
        (long long)(mrca_now_ms() - m->session_start_ms));
}
