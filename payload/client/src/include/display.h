#ifndef MRCA_DISPLAY_H
#define MRCA_DISPLAY_H
#include "mrca.h"
#include <stddef.h>

struct display_surface {
    int      kind;             /* 0=none 1=fbdev 2=drm */
    int      fd;
    void    *map;
    size_t   map_len;
    uint32_t width, height;
    uint32_t stride;           /* bytes per line */
    uint32_t bpp;
    uint8_t  pixfmt;
    int      drm_fd;
    uint32_t drm_crtc, drm_conn, drm_fb, drm_dumb_handle;
    uint32_t drm_w, drm_h, drm_pitch;
    int      active;
};

int  display_open(struct display_surface *d, int prefer_drm);
int  display_present_rect(struct display_surface *d, const void *pixels,
                          uint32_t fmt, const struct mrca_rect *r,
                          uint32_t src_stride);
int  display_present_full(struct display_surface *d, const void *pixels,
                          uint32_t fmt, uint32_t w, uint32_t h, uint32_t src_stride);
void display_close(struct display_surface *d);

#endif
