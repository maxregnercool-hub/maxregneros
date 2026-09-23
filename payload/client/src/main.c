/* main.c — session loop.
 *
 * Layout of a live session:
 *
 *   thread A (control)   : upstream input + 1Hz stats, downstream control ops
 *   thread B (stream)    : downstream frame payloads -> ring -> display
 *   thread C (display)   : drains the ring and blits, pets the watchdog
 *
 * Two sockets, not one. The reason is head-of-line blocking, not elegance: on
 * a single multiplexed connection a 300KB dirty-frame payload sits in the send
 * queue ahead of the next touch event, and at 60fps that is 5ms of avoidable
 * input latency. Control traffic gets its own socket with TCP_NODELAY.
 */
#include "mrca.h"
#include "config.h"
#include "log.h"
#include "util.h"
#include "net.h"
#include "ws.h"
#include "proto.h"
#include "input.h"
#include "display.h"
#include "codec.h"
#include "metrics.h"
#include "watchdog.h"
#include "ring.h"
#include "audio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

static volatile int  g_running = 1;
static struct mrca_config   g_cfg;
static struct mrca_metrics  g_metrics;
static struct display_surface g_disp;
static struct input_ctx     g_input;
static struct mrca_ring     g_frames;
static struct watchdog      g_wd;
static struct audio_ctx     g_audio;

static pthread_mutex_t g_ctl_lock = PTHREAD_MUTEX_INITIALIZER;
static struct ws_conn  g_ctl;
static int             g_ctl_ok = 0;
static char            g_session_id[64];
static uint16_t        g_remote_w, g_remote_h;
static uint8_t         g_remote_pixfmt = PF_XRGB8888;
static uint32_t        g_frame_seq = 0;
static int64_t         g_last_stats_ms = 0;

static void on_signal(int sig) {
    (void)sig;
    g_running = 0;
}

static void on_watchdog_trip(void) {
    LOGI("watchdog trip: aborting session");
    g_running = 0;
    _exit(70);
}

/* ── sending helpers ─────────────────────────────────────────── */

static int ctl_send_raw(const uint8_t *buf, size_t len) {
    pthread_mutex_lock(&g_ctl_lock);
    int rc = MRCA_ERR_STATE;
    if (g_ctl_ok) {
        rc = (ws_send(&g_ctl, WS_BIN, buf, len) == MRCA_OK) ? MRCA_OK : MRCA_ERR_IO;
        if (rc != MRCA_OK) g_ctl_ok = 0;
    }
    pthread_mutex_unlock(&g_ctl_lock);
    return rc;
}

static void send_stats(void) {
    char json[512];
    metrics_to_json(&g_metrics, json, sizeof json);
    uint8_t *pkt = NULL; size_t n = 0;
    if (proto_pack_stat(json, &pkt, &n) == MRCA_OK) {
        ctl_send_raw(pkt, n);
        free(pkt);
    }
}

static void send_keyframe_request(void) {
    struct mrca_hdr h;
    memset(&h, 0, sizeof h);
    h.magic[0]='M'; h.magic[1]='R';
    h.version = MRCA_PROTO_VERSION;
    h.op = OP_KEYFRAME;
    uint8_t buf[MRCA_HDR_SIZE];
    mrca_hdr_pack(&h, buf);
    ctl_send_raw(buf, sizeof buf);
}

/* ── thread C: display drain ─────────────────────────────────── */
static void *display_thread(void *arg) {
    (void)arg;
    uint8_t *scratch = malloc(MRCA_MAX_FRAME_BYTES);
    if (!scratch) { LOGE("display thread OOM"); return NULL; }

    while (g_running) {
        size_t avail = mrca_ring_avail(&g_frames);
        if (avail < 4) { mrca_sleep_ms(2); watchdog_pet(&g_wd); continue; }

        uint8_t hdr4[4];
        if (mrca_ring_peek(&g_frames, hdr4, 4) != 4) { mrca_sleep_ms(1); continue; }
        uint32_t plen = (uint32_t)hdr4[0] | ((uint32_t)hdr4[1] << 8) |
                        ((uint32_t)hdr4[2] << 16) | ((uint32_t)hdr4[3] << 24);
        if (plen > MRCA_MAX_FRAME_BYTES) {
            LOGI("bogus frame length %u; resetting ring", plen);
            mrca_ring_reset(&g_frames);
            continue;
        }
        if (avail < 4 + plen) { mrca_sleep_ms(2); watchdog_pet(&g_wd); continue; }

        mrca_ring_read(&g_frames, scratch, 4 + plen);
        struct codec_frame cf;
        memset(&cf, 0, sizeof cf);
        cf.payload = scratch + 4;
        cf.len     = plen;
        cf.codec   = CODEC_RAW;
        cf.pixfmt  = g_remote_pixfmt;

        uint64_t t0 = (uint64_t)mrca_now_us();
        int rc = codec_decode_into(&g_disp, &cf);
        uint64_t t1 = (uint64_t)mrca_now_us();
        if (rc == MRCA_OK) metrics_note_blit(&g_metrics, (t1 - t0) * 1000ull);
        else g_metrics.frames_dropped++;
        watchdog_pet(&g_wd);
    }
    free(scratch);
    return NULL;
}

/* ── thread B: stream socket reader ──────────────────────────── */
static void *stream_thread(void *arg) {
    (void)arg;
    int backoff = 250;

    while (g_running) {
        struct ws_conn ws;
        memset(&ws, 0, sizeof ws);
        char path[256];
        snprintf(path, sizeof path, "/stream/%s?token=%s",
                 g_session_id[0] ? g_session_id : "pending",
                 g_cfg.auth_token);

        int rc = ws_connect(&ws, g_cfg.host, g_cfg.stream_port, path,
                            g_cfg.host, NULL, 5000);
        if (rc != MRCA_OK) {
            LOGD("stream connect failed (%s), retry in %dms", mrca_status_str(rc), backoff);
            mrca_sleep_ms(backoff);
            backoff = backoff * 2;
            if (backoff > 15000) backoff = 15000;
            g_metrics.reconnect_count++;
            continue;
        }
        backoff = 250;
        g_metrics.connect_count++;
        LOGI("stream channel up");

        while (g_running) {
            int op = -1; uint8_t *payload = NULL; size_t len = 0;
            rc = ws_recv(&ws, &op, &payload, &len);
            if (rc != MRCA_OK) {
                LOGD("stream recv: %s", mrca_status_str(rc));
                free(payload);
                break;
            }
            if (op == WS_PING) { ws_send(&ws, WS_PONG, payload, len); free(payload); continue; }
            if (op == WS_PONG) { free(payload); continue; }
            if (op == WS_CLOSE) { free(payload); break; }
            if (op != WS_BIN || len < MRCA_HDR_SIZE) { free(payload); continue; }

            uint8_t fop, fflags; uint32_t seq, plen;
            if (proto_parse_frame_hdr(payload, len, &fop, &fflags, &seq, &plen) != MRCA_OK) {
                free(payload); continue;
            }
            if (fop != OP_FRAME) { free(payload); continue; }
            if (plen != len - MRCA_HDR_SIZE) {
                LOGD("frame length mismatch: hdr=%u actual=%zu", plen, len - MRCA_HDR_SIZE);
                free(payload); continue;
            }

            uint8_t lenb[4];
            lenb[0] = (uint8_t)(plen & 0xff);
            lenb[1] = (uint8_t)((plen >> 8) & 0xff);
            lenb[2] = (uint8_t)((plen >> 16) & 0xff);
            lenb[3] = (uint8_t)((plen >> 24) & 0xff);
            mrca_ring_write(&g_frames, lenb, 4);
            size_t wrote = mrca_ring_write(&g_frames, payload + MRCA_HDR_SIZE, plen);
            if (wrote != plen) g_metrics.frames_dropped++;

            g_frame_seq = seq;
            metrics_note_frame(&g_metrics, len, 0);
            free(payload);
        }

        ws_close(&ws);
        if (!g_running) break;
        g_metrics.reconnect_count++;
        LOGI("stream channel down; reconnecting");
        mrca_sleep_ms(backoff);
    }
    return NULL;
}

/* ── thread A: control socket ────────────────────────────────── */

static void handle_control_message(int op, const uint8_t *payload, size_t len) {
    switch (op) {
        case OP_WELCOME: {
            struct mrca_welcome w;
            if (proto_parse_welcome(payload, len, &w) == MRCA_OK) {
                snprintf(g_session_id, sizeof g_session_id, "%s", w.session_id);
                g_remote_w = w.remote_w;
                g_remote_h = w.remote_h;
                if (w.pixfmt) g_remote_pixfmt = w.pixfmt;
                input_set_remote_size(&g_input, w.remote_w, w.remote_h);
                LOGI("session %s canvas %ux%u fmt=%s",
                     w.session_id, w.remote_w, w.remote_h,
                     codec_pixfmt_name(w.pixfmt));
                send_keyframe_request();
            } else {
                LOGI("malformed WELCOME");
            }
            break;
        }
        case OP_QUALITY:
            LOGD("quality hint: %.*s", (int)len, (const char *)payload);
            break;
        case OP_PING:
            ctl_send_raw(payload, len);
            break;
        case OP_CLIPBOARD:
            LOGD("clipboard payload %zu bytes", len);
            break;
        case OP_AUDIO:
            audio_submit(&g_audio, payload, len);
            break;
        case OP_BYE:
            LOGI("server said goodbye");
            g_running = 0;
            break;
        default:
            LOGT("control op %s len=%zu", mrca_op_str(op), len);
            break;
    }
}

static int control_connect(void) {
    char path[256];
    snprintf(path, sizeof path, "/control?token=%s&device=%s",
             g_cfg.auth_token, g_cfg.device_id);

    pthread_mutex_lock(&g_ctl_lock);
    int rc = ws_connect(&g_ctl, g_cfg.host, g_cfg.control_port, path,
                        g_cfg.host, NULL, 5000);
    g_ctl_ok = (rc == MRCA_OK) ? 1 : 0;
    pthread_mutex_unlock(&g_ctl_lock);
    if (rc != MRCA_OK) return rc;
    g_metrics.connect_count++;

    struct mrca_hello h;
    memset(&h, 0, sizeof h);
    snprintf(h.device_id, sizeof h.device_id, "%s", g_cfg.device_id);
    snprintf(h.token, sizeof h.token, "%s", g_cfg.auth_token);
    h.proto = MRCA_PROTO_VERSION;
    h.local_w = (uint16_t)g_disp.width;
    h.local_h = (uint16_t)g_disp.height;
    h.density_dpi = 0;
    snprintf(h.model, sizeof h.model, "%s", config_guess_model());

    uint8_t caps = 0;
    if (g_input.ndev > 0) caps |= CAP_MT | CAP_KEY | CAP_SCROLL;
    if (g_audio.enabled)  caps |= CAP_AUDIO;
    h.caps = caps;

    uint8_t *pkt = NULL; size_t n = 0;
    if (proto_pack_hello(&h, &pkt, &n) != MRCA_OK) return MRCA_ERR_GENERIC;

    pthread_mutex_lock(&g_ctl_lock);
    rc = ws_send(&g_ctl, WS_BIN, pkt, n);
    pthread_mutex_unlock(&g_ctl_lock);
    free(pkt);
    if (rc != MRCA_OK) { g_ctl_ok = 0; return MRCA_ERR_IO; }

    LOGI("control channel up, HELLO sent (%s)", h.device_id);
    return MRCA_OK;
}


/* ── control-channel drain ─────────────────────────────────────────
 * The control socket is where WELCOME / QUALITY / AUDIO / BYE arrive.
 * Without this the session id is never learned and audio never starts.
 */
static void ctl_drain(void) {
    for (int guard = 0; guard < 32; guard++) {
        int op = -1; uint8_t *payload = NULL; size_t len = 0;

        pthread_mutex_lock(&g_ctl_lock);
        int rc = g_ctl_ok ? ws_recv_timeout(&g_ctl, &op, &payload, &len, 2)
                          : MRCA_ERR_STATE;
        pthread_mutex_unlock(&g_ctl_lock);

        if (rc == MRCA_ERR_TIMEOUT || rc == MRCA_ERR_AGAIN) { free(payload); return; }
        if (rc != MRCA_OK) { free(payload); return; }

        if (op == WS_PING) {
            pthread_mutex_lock(&g_ctl_lock);
            if (g_ctl_ok) ws_send(&g_ctl, WS_PONG, payload, len);
            pthread_mutex_unlock(&g_ctl_lock);
            free(payload); continue;
        }
        if (op == WS_CLOSE) { free(payload); return; }
        if (op != WS_BIN || len < MRCA_HDR_SIZE) { free(payload); continue; }

        uint8_t fop = 0, fflags = 0;
        uint32_t seq = 0, plen = 0;
        if (proto_parse_frame_hdr(payload, len, &fop, &fflags, &seq, &plen) != MRCA_OK) {
            free(payload); continue;
        }
        size_t avail = len - MRCA_HDR_SIZE;
        handle_control_message(fop, payload + MRCA_HDR_SIZE,
                               (plen < avail) ? plen : avail);
        free(payload);
    }
}

int main(int argc, char **argv) {
    const char *conf_path = "/data/adb/maxregner/ca.conf";
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--config") && i + 1 < argc) conf_path = argv[++i];
        else if (!strcmp(argv[i], "--version")) {
            printf("mrca-client %d.%d.%d proto %d\n",
                   MRCA_VERSION_MAJOR, MRCA_VERSION_MINOR, MRCA_VERSION_PATCH,
                   MRCA_PROTO_VERSION);
            return 0;
        } else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
            printf("usage: mrca-client [--config PATH] [--version]\n");
            return 0;
        }
    }

    signal(SIGPIPE, SIG_IGN);
    signal(SIGTERM, on_signal);
    signal(SIGINT,  on_signal);

    config_load(&g_cfg, conf_path);
    mrca_log_init(g_cfg.log_level, g_cfg.log_file);
    LOGI("mrca-client %d.%d.%d starting", MRCA_VERSION_MAJOR,
         MRCA_VERSION_MINOR, MRCA_VERSION_PATCH);

    if (!g_cfg.enabled) { LOGI("disabled by config; exiting"); return 0; }

    int conf_dirty = 0;
    if (!g_cfg.device_id[0]) {
        char seed[40];
        snprintf(seed, sizeof seed, "%lld-%d", (long long)mrca_now_us(), (int)getpid());
        mrca_rand_seed(mrca_crc32(seed, strlen(seed)));
        mrca_gen_token(g_cfg.device_id, 24);
        conf_dirty = 1;
    }
    if (!g_cfg.auth_token[0]) {
        mrca_gen_token(g_cfg.auth_token, 48);
        conf_dirty = 1;
    }
    if (conf_dirty) config_save(&g_cfg, conf_path);

    metrics_reset(&g_metrics);

    if (display_open(&g_disp, g_cfg.prefer_drm) != MRCA_OK) {
        LOGE("cannot open a display surface; aborting");
        return 2;
    }

    int32_t lw = g_cfg.local_w ? g_cfg.local_w : (int32_t)g_disp.width;
    int32_t lh = g_cfg.local_h ? g_cfg.local_h : (int32_t)g_disp.height;
    input_init(&g_input, lw, lh, lw, lh);

    audio_init(&g_audio, NULL);
    mrca_ring_init(&g_frames, MRCA_MAX_FRAME_BYTES * 2);
    watchdog_start(&g_wd, 8000, on_watchdog_trip);

    pthread_t t_disp, t_stream;
    pthread_create(&t_disp, NULL, display_thread, NULL);
    pthread_create(&t_stream, NULL, stream_thread, NULL);

    int backoff = g_cfg.reconnect_min_ms;
    while (g_running) {
        if (!g_ctl_ok) {
            if (control_connect() != MRCA_OK) {
                LOGD("control connect failed; retry in %dms", backoff);
                mrca_sleep_ms(backoff);
                backoff = backoff * 2;
                if (backoff > g_cfg.reconnect_max_ms) backoff = g_cfg.reconnect_max_ms;
                g_metrics.reconnect_count++;
                watchdog_pet(&g_wd);
                continue;
            }
            backoff = g_cfg.reconnect_min_ms;
        }

        struct mrca_event evs[MRCA_MAX_EVENTS];
        int n = input_poll(&g_input, evs, MRCA_MAX_EVENTS, 4);
        if (n > 0) {
            uint8_t *pkt = NULL; size_t plen = 0;
            if (proto_pack_input(evs, n, &pkt, &plen) == MRCA_OK) {
                if (ctl_send_raw(pkt, plen) == MRCA_OK)
                    for (int i = 0; i < n; i++) metrics_note_event(&g_metrics);
                free(pkt);
            }
        }

        int64_t now = mrca_now_ms();
        if (now - g_last_stats_ms >= 1000) {
            g_last_stats_ms = now;
            metrics_tick(&g_metrics);
            send_stats();

            struct mrca_hdr ph;
            memset(&ph, 0, sizeof ph);
            ph.magic[0]='M'; ph.magic[1]='R';
            ph.version = MRCA_PROTO_VERSION;
            ph.op = OP_PING;
            uint8_t pb[MRCA_HDR_SIZE];
            mrca_hdr_pack(&ph, pb);
            ctl_send_raw(pb, sizeof pb);
        }

        ctl_drain();
        watchdog_pet(&g_wd);
    }

    LOGI("shutting down");
    watchdog_stop(&g_wd);

    pthread_mutex_lock(&g_ctl_lock);
    if (g_ctl_ok) ws_close(&g_ctl);
    g_ctl_ok = 0;
    pthread_mutex_unlock(&g_ctl_lock);

    pthread_join(t_stream, NULL);
    pthread_join(t_disp, NULL);

    audio_shutdown(&g_audio);
    input_shutdown(&g_input);
    display_close(&g_disp);
    mrca_ring_free(&g_frames);
    LOGI("bye");
    mrca_log_close();
    return 0;
}
