/* input.c — Layer A "Input Event Collector".
 *
 * Enumerates /dev/input/event*, classifies each node by the capability bits in
 * its EVIOCGBIT report, then multiplexes them all through a single poll loop.
 * Touch is translated from the panel's own ABS range into REMOTE canvas space
 * here, at the edge, so the cloud session never needs to know the client's
 * panel geometry and a resolution change costs one multiplication instead of
 * a round trip.
 */
#include "input.h"
#include "log.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <linux/input.h>

#ifndef INPUT_PROP_DIRECT
#define INPUT_PROP_DIRECT 0x01
#endif
#ifndef ABS_MT_SLOT
#define ABS_MT_SLOT 0x2f
#endif
#ifndef ABS_MT_POSITION_X
#define ABS_MT_POSITION_X 0x35
#endif
#ifndef ABS_MT_POSITION_Y
#define ABS_MT_POSITION_Y 0x36
#endif
#ifndef ABS_MT_TRACKING_ID
#define ABS_MT_TRACKING_ID 0x39
#endif

#define LONG_BITS (sizeof(long) * 8)
#define NLONGS(x) (((x) + LONG_BITS - 1) / LONG_BITS)

static int test_bit(const unsigned long *bits, int bit) {
    return (bits[bit / LONG_BITS] >> (bit % LONG_BITS)) & 1UL;
}

static int has_cap(int fd, int ev, unsigned long *out, int nlongs) {
    memset(out, 0, nlongs * sizeof(long));
    if (ioctl(fd, EVIOCGBIT(ev, nlongs * sizeof(long)), out) < 0) return 0;
    return 1;
}

static void classify(struct input_dev *d) {
    unsigned long absbits[NLONGS(ABS_MAX + 1)] = {0};
    unsigned long keybits[NLONGS(KEY_MAX + 1)] = {0};
    unsigned long relbits[NLONGS(REL_MAX + 1)] = {0};

    d->has_abs  = has_cap(d->fd, EV_ABS,  absbits, NLONGS(ABS_MAX + 1));
    d->has_keys = has_cap(d->fd, EV_KEY,  keybits, NLONGS(KEY_MAX + 1));
    d->has_rel  = has_cap(d->fd, EV_REL,  relbits, NLONGS(REL_MAX + 1));

    if (d->has_abs && test_bit(absbits, ABS_MT_POSITION_X) &&
                      test_bit(absbits, ABS_MT_POSITION_Y)) {
        d->has_mt = 1;
        struct input_absinfo ai;
        if (ioctl(d->fd, EVIOCGABS(ABS_MT_POSITION_X), &ai) == 0) {
            d->abs_x_min = ai.minimum; d->abs_x_max = ai.maximum;
        }
        if (ioctl(d->fd, EVIOCGABS(ABS_MT_POSITION_Y), &ai) == 0) {
            d->abs_y_min = ai.minimum; d->abs_y_max = ai.maximum;
        }
    } else if (d->has_abs && test_bit(absbits, ABS_X) && test_bit(absbits, ABS_Y)) {
        /* single-touch resistive / older digitizer */
        d->has_mt = 1;
        struct input_absinfo ai;
        if (ioctl(d->fd, EVIOCGABS(ABS_X), &ai) == 0) {
            d->abs_x_min = ai.minimum; d->abs_x_max = ai.maximum;
        }
        if (ioctl(d->fd, EVIOCGABS(ABS_Y), &ai) == 0) {
            d->abs_y_min = ai.minimum; d->abs_y_max = ai.maximum;
        }
    }
    if (d->has_keys && test_bit(keybits, BTN_TOUCH)) {
        /* it's a touch device even if the MT axes look odd */
    }
    if (d->has_keys) {
        int any_key = 0;
        for (int k = KEY_ESC; k <= KEY_KPDOT; k++)
            if (test_bit(keybits, k)) { any_key = 1; break; }
        if (!any_key)
            for (int k = BTN_MISC; k <= BTN_GEAR_UP; k++)
                if (test_bit(keybits, k)) { any_key = 1; break; }
        d->has_keys = any_key;
    }
    LOGD("input %s name='%s' mt=%d keys=%d rel=%d", d->path, d->name,
         d->has_mt, d->has_keys, d->has_rel);
}

int input_init(struct input_ctx *ic, int32_t src_w, int32_t src_h,
               int32_t dst_w, int32_t dst_h) {
    memset(ic, 0, sizeof *ic);
    ic->src_w = src_w; ic->src_h = src_h;
    ic->dst_w = dst_w; ic->dst_h = dst_h;
    for (int i = 0; i < INPUT_MAX_DEV; i++) ic->dev[i].fd = -1;

    DIR *dir = opendir("/dev/input");
    if (!dir) {
        LOGI("no /dev/input — running without local input");
        return MRCA_OK;
    }
    struct dirent *de;
    while ((de = readdir(dir)) != NULL && ic->ndev < INPUT_MAX_DEV) {
        if (strncmp(de->d_name, "event", 5) != 0) continue;
        char path[64];
        snprintf(path, sizeof path, "/dev/input/%.40s", de->d_name);
        int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) continue;
        struct input_dev *d = &ic->dev[ic->ndev];
        memset(d, 0, sizeof *d);
        d->fd = fd;
        snprintf(d->path, sizeof d->path, "%s", path);
        ioctl(fd, EVIOCGNAME(sizeof d->name - 1), d->name);
        struct input_id id;
        if (ioctl(fd, EVIOCGID, &id) == 0) {
            d->id_bustype = id.bustype;
            d->id_vendor  = id.vendor;
            d->id_product = id.product;
        }
        classify(d);
        if (!d->has_mt && !d->has_keys && !d->has_rel) {
            close(fd); d->fd = -1;
            continue;
        }
        ic->ndev++;
    }
    closedir(dir);
    LOGI("input: %d device(s) online", ic->ndev);
    return MRCA_OK;
}

void input_shutdown(struct input_ctx *ic) {
    for (int i = 0; i < ic->ndev; i++)
        if (ic->dev[i].fd >= 0) { close(ic->dev[i].fd); ic->dev[i].fd = -1; }
    ic->ndev = 0;
}

void input_set_remote_size(struct input_ctx *ic, int32_t w, int32_t h) {
    ic->dst_w = w; ic->dst_h = h;
    LOGD("input: remote canvas %dx%d", w, h);
}

static int32_t scale_axis(int32_t v, int32_t lo, int32_t hi, int32_t out) {
    if (hi <= lo) return 0;
    if (v < lo) v = lo;
    if (v > hi) v = hi;
    int64_t num = (int64_t)(v - lo) * out;
    return (int32_t)(num / (hi - lo));
}

static void map_touch(struct input_ctx *ic, struct input_dev *d,
                      int32_t lx, int32_t ly, int32_t *ox, int32_t *oy) {
    int32_t sx = scale_axis(lx, d->abs_x_min, d->abs_x_max, ic->src_w ? ic->src_w : 1);
    int32_t sy = scale_axis(ly, d->abs_y_min, d->abs_y_max, ic->src_h ? ic->src_h : 1);
    int32_t rx = ic->src_w  > 0 ? (int32_t)((int64_t)sx * ic->dst_w / ic->src_w) : sx;
    int32_t ry = ic->src_h  > 0 ? (int32_t)((int64_t)sy * ic->dst_h / ic->src_h) : sy;
    if (rx < 0) rx = 0;
    if (rx >= ic->dst_w) rx = ic->dst_w - 1;
    if (ry < 0) ry = 0;
    if (ry >= ic->dst_h) ry = ic->dst_h - 1;
    *ox = rx; *oy = ry;
}

static int emit(struct mrca_event *out, int n, int max, uint8_t cls, uint8_t slot,
                uint16_t code, int32_t x, int32_t y, int32_t value) {
    if (n >= max) return n;
    struct mrca_event *e = &out[n];
    e->cls = cls; e->slot = slot; e->code = code;
    e->x = x; e->y = y; e->value = value;
    e->ts_ms = (uint32_t)mrca_now_ms();
    return n + 1;
}

int input_poll(struct input_ctx *ic, struct mrca_event *out, int max, int timeout_ms) {
    if (ic->ndev <= 0) { mrca_sleep_ms(timeout_ms > 0 ? timeout_ms : 50); return 0; }

    struct pollfd pfd[INPUT_MAX_DEV];
    int idxmap[INPUT_MAX_DEV];
    int np = 0;
    for (int i = 0; i < ic->ndev; i++) {
        if (ic->dev[i].fd < 0) continue;
        pfd[np].fd = ic->dev[i].fd;
        pfd[np].events = POLLIN;
        pfd[np].revents = 0;
        idxmap[np] = i;
        np++;
    }
    if (np == 0) return 0;

    int pr = poll(pfd, np, timeout_ms);
    if (pr <= 0) return 0;

    int nevents = 0;
    for (int p = 0; p < np && nevents < max; p++) {
        if (!(pfd[p].revents & POLLIN)) continue;
        struct input_dev *d = &ic->dev[idxmap[p]];
        struct input_event evs[64];
        ssize_t rd = read(d->fd, evs, sizeof evs);
        if (rd <= 0) continue;
        int n = (int)(rd / sizeof(struct input_event));

        for (int i = 0; i < n && nevents < max; i++) {
            struct input_event *ev = &evs[i];
            if (ev->type == EV_SYN) {
                continue;
            }
            if (ev->type == EV_ABS) {
                if (ev->code == ABS_MT_SLOT) {
                    d->cur_slot = ev->value;
                    if (d->cur_slot < 0) d->cur_slot = 0;
                    if (d->cur_slot > 15) d->cur_slot = 15;
                } else if (ev->code == ABS_MT_TRACKING_ID) {
                    int s = d->cur_slot;
                    if (ev->value == -1) {
                        if (d->down[s]) {
                            int32_t rx, ry;
                            map_touch(ic, d, d->slot_x[s], d->slot_y[s], &rx, &ry);
                            nevents = emit(out, nevents, max, EV_MT_UP, (uint8_t)s,
                                           (uint16_t)s, rx, ry, 0);
                            d->down[s] = 0;
                        }
                    } else {
                        d->down[s] = 1;
                        int32_t rx, ry;
                        map_touch(ic, d, d->slot_x[s], d->slot_y[s], &rx, &ry);
                        nevents = emit(out, nevents, max, EV_MT_DOWN, (uint8_t)s,
                                       (uint16_t)s, rx, ry, 255);
                    }
                } else if (ev->code == ABS_MT_POSITION_X || ev->code == ABS_X) {
                    int s = d->cur_slot;
                    d->slot_x[s] = ev->value;
                    d->last_x = ev->value;
                } else if (ev->code == ABS_MT_POSITION_Y || ev->code == ABS_Y) {
                    int s = d->cur_slot;
                    d->slot_y[s] = ev->value;
                    d->last_y = ev->value;
                } else if (ev->code == ABS_MT_PRESSURE || ev->code == ABS_PRESSURE) {
                    /* carried on the next move event */
                }
            } else if (ev->type == EV_KEY) {
                if (ev->code == BTN_TOUCH) {
                    if (ev->value == 1) {
                        int s = d->cur_slot;
                        int32_t rx, ry;
                        map_touch(ic, d, d->last_x, d->last_y, &rx, &ry);
                        d->slot_x[s] = d->last_x; d->slot_y[s] = d->last_y;
                        d->down[s] = 1;
                        nevents = emit(out, nevents, max, EV_MT_DOWN, (uint8_t)s,
                                       (uint16_t)s, rx, ry, 255);
                    } else {
                        for (int s = 0; s < 16; s++) {
                            if (!d->down[s]) continue;
                            int32_t rx, ry;
                            map_touch(ic, d, d->slot_x[s], d->slot_y[s], &rx, &ry);
                            nevents = emit(out, nevents, max, EV_MT_UP, (uint8_t)s,
                                           (uint16_t)s, rx, ry, 0);
                            d->down[s] = 0;
                        }
                    }
                } else if (ev->code == BTN_LEFT) {
                    d->btn_left = ev->value;
                    int32_t rx, ry;
                    map_touch(ic, d, d->last_x, d->last_y, &rx, &ry);
                    nevents = emit(out, nevents, max,
                                   ev->value ? EV_MT_DOWN : EV_MT_UP, 0,
                                   ev->code, rx, ry, ev->value ? 255 : 0);
                } else {
                    nevents = emit(out, nevents, max, EV_KEY, 0,
                                   (uint16_t)ev->code, 0, 0, ev->value);
                }
            } else if (ev->type == EV_REL) {
                if (ev->code == REL_WHEEL || ev->code == REL_HWHEEL) {
                    int32_t rx, ry;
                    map_touch(ic, d, d->last_x, d->last_y, &rx, &ry);
                    nevents = emit(out, nevents, max, EV_SCROLL, 0,
                                   (uint16_t)ev->code, rx, ry, ev->value);
                } else if (ev->code == REL_X) {
                    d->last_x += ev->value;
                } else if (ev->code == REL_Y) {
                    d->last_y += ev->value;
                }
            }
        }

        /* After a SYN_REPORT, push a MOVED event for every slot still down so
         * the cloud pointer tracks a drag even when only the position axes
         * changed without a tracking-id transition. */
        for (int s = 0; s < 16 && nevents < max; s++) {
            if (!d->down[s]) continue;
            int32_t rx, ry;
            map_touch(ic, d, d->slot_x[s], d->slot_y[s], &rx, &ry);
            nevents = emit(out, nevents, max, EV_MT_MOVE, (uint8_t)s,
                           (uint16_t)s, rx, ry, 255);
        }
    }
    return nevents;
}
