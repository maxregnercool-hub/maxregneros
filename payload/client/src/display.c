/* display.c — Layer A "Display Pipeline Interface".
 *
 * Deliberately has no window system. Two back-ends:
 *   - legacy fbdev  (/dev/graphics/fb0 or /dev/fb0)
 *   - DRM/KMS       (/dev/dri/cardN)
 * Both are mmap-and-write: the incoming frame payload is copied into the
 * mapped scanout buffer and presented. There is no compositor in the path,
 * which is exactly why the local device can be a 350MB base system.
 */
#include "display.h"
#include "drm.h"
#include "log.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <linux/kd.h>

#ifndef FBIOPAN_DISPLAY
#define FBIOPAN_DISPLAY 0x4606
#endif

static const char *fb_candidates[] = {
    "/dev/graphics/fb0", "/dev/fb0", "/dev/fb/0", NULL
};

static uint8_t fb_pixfmt_from_var(const struct fb_var_screeninfo *v) {
    if (v->bits_per_pixel == 16) return PF_RGB565;
    if (v->bits_per_pixel == 32) {
        if (v->transp.length == 8) return PF_RGBA8888;
        return PF_XRGB8888;
    }
    return PF_UNKNOWN;
}

static int display_open_fbdev(struct display_surface *d) {
    for (int i = 0; fb_candidates[i]; i++) {
        int fd = open(fb_candidates[i], O_RDWR);
        if (fd < 0) continue;

        struct fb_fix_screeninfo fix;
        struct fb_var_screeninfo var;
        if (ioctl(fd, FBIOGET_FSCREENINFO, &fix) != 0 ||
            ioctl(fd, FBIOGET_VSCREENINFO, &var) != 0) {
            close(fd); continue;
        }
        size_t maplen = (size_t)fix.line_length * var.yres_virtual;
        if (maplen == 0) maplen = (size_t)fix.line_length * var.yres;
        void *map = mmap(NULL, maplen, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (map == MAP_FAILED) { close(fd); continue; }

        memset(d, 0, sizeof *d);
        d->kind    = 1;
        d->fd      = fd;
        d->map     = map;
        d->map_len = maplen;
        d->width   = var.xres;
        d->height  = var.yres;
        d->stride  = fix.line_length;
        d->bpp     = var.bits_per_pixel;
        d->pixfmt  = fb_pixfmt_from_var(&var);
        d->active  = 1;
        LOGI("display: fbdev %s %ux%u %ubpp stride=%u fmt=%s",
             fb_candidates[i], d->width, d->height, d->bpp, d->stride,
             d->pixfmt == PF_RGB565 ? "RGB565" :
             d->pixfmt == PF_RGBA8888 ? "RGBA8888" : "XRGB8888");

        /* Blanking off any console takeover that may be sitting on the panel. */
        int tty = open("/dev/tty0", O_RDWR);
        if (tty >= 0) {
            ioctl(tty, KDSETMODE, KD_GRAPHICS);
            close(tty);
        }
        return MRCA_OK;
    }
    return MRCA_ERR_IO;
}

int display_open(struct display_surface *d, int prefer_drm) {
    memset(d, 0, sizeof *d);
    d->fd = -1;
    if (prefer_drm) {
        extern int drm_open(struct display_surface *);
        if (drm_open(d) == MRCA_OK) return MRCA_OK;
        LOGD("drm unavailable, falling back to fbdev");
    }
    if (display_open_fbdev(d) == MRCA_OK) return MRCA_OK;
    if (!prefer_drm) {
        extern int drm_open(struct display_surface *);
        if (drm_open(d) == MRCA_OK) return MRCA_OK;
    }
    LOGI("display: no usable framebuffer found");
    return MRCA_ERR_IO;
}

/* Blit one rectangle. Both the source and destination are little-endian
 * packed pixels; when the formats already match this is a plain row copy,
 * when they don't we do the per-pixel swizzle in place. */
int display_present_rect(struct display_surface *d, const void *pixels,
                         uint32_t fmt, const struct mrca_rect *r,
                         uint32_t src_stride) {
    if (!d->active || !d->map) return MRCA_ERR_STATE;
    if (r->w <= 0 || r->h <= 0) return MRCA_OK;

    int32_t x = r->x, y = r->y, w = r->w, h = r->h;
    mrca_clamp_rect(&x, &y, &w, &h, (int32_t)d->width, (int32_t)d->height);
    if (w <= 0 || h <= 0) return MRCA_OK;

    const uint8_t *src_base = (const uint8_t *)pixels;
    uint32_t sbpp = (fmt == PF_RGB565) ? 2 : 4;
    uint32_t sstride = src_stride ? src_stride : (uint32_t)w * sbpp;
    uint32_t dbpp_bytes = d->bpp / 8;

    for (int32_t row = 0; row < h; row++) {
        const uint8_t *sp = src_base + (size_t)row * sstride;
        uint8_t *dp = (uint8_t *)d->map + (size_t)(y + row) * d->stride +
                      (size_t)x * dbpp_bytes;

        if (fmt == d->pixfmt) {
            memcpy(dp, sp, (size_t)w * sbpp);
            continue;
        }
        /* format conversion */
        if (fmt == PF_XRGB8888 && d->pixfmt == PF_RGB565) {
            const uint32_t *s32 = (const uint32_t *)sp;
            uint16_t *d16 = (uint16_t *)dp;
            for (int32_t i = 0; i < w; i++) {
                uint32_t px = s32[i];
                uint16_t rr = (uint16_t)((px >> 19) & 0x1F);
                uint16_t gg = (uint16_t)((px >> 10) & 0x3F);
                uint16_t bb = (uint16_t)((px >> 3)  & 0x1F);
                d16[i] = (uint16_t)((rr << 11) | (gg << 5) | bb);
            }
        } else if (fmt == PF_RGB565 && d->pixfmt == PF_XRGB8888) {
            const uint16_t *s16 = (const uint16_t *)sp;
            uint32_t *d32 = (uint32_t *)dp;
            for (int32_t i = 0; i < w; i++) {
                uint16_t px = s16[i];
                uint32_t rr = (px >> 11) & 0x1F;
                uint32_t gg = (px >> 5)  & 0x3F;
                uint32_t bb = px & 0x1F;
                rr = (rr << 3) | (rr >> 2);
                gg = (gg << 2) | (gg >> 4);
                bb = (bb << 3) | (bb >> 2);
                d32[i] = 0xFF000000u | (rr << 16) | (gg << 8) | bb;
            }
        } else {
            /* Unknown pairing: copy what we can so the screen still updates. */
            size_t n = (size_t)w * sbpp;
            if (n > (size_t)w * dbpp_bytes) n = (size_t)w * dbpp_bytes;
            memcpy(dp, sp, n);
        }
    }
    return MRCA_OK;
}

int display_present_full(struct display_surface *d, const void *pixels,
                         uint32_t fmt, uint32_t w, uint32_t h, uint32_t src_stride) {
    struct mrca_rect r = { 0, 0, (int32_t)w, (int32_t)h };
    int rc = display_present_rect(d, pixels, fmt, &r, src_stride);
    if (rc != MRCA_OK) return rc;
    if (d->kind == 1) {
        struct fb_var_screeninfo var;
        if (ioctl(d->fd, FBIOGET_VSCREENINFO, &var) == 0) {
            var.yoffset = 0;
            ioctl(d->fd, FBIOPAN_DISPLAY, &var);
        }
    }
    return MRCA_OK;
}

void display_close(struct display_surface *d) {
    if (!d) return;
    if (d->map && d->map != MAP_FAILED) munmap(d->map, d->map_len);
    d->map = NULL;
    if (d->fd >= 0) { close(d->fd); d->fd = -1; }
    d->active = 0;
}
