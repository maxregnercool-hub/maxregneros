/* drm.c — DRM/KMS back-end. Talks the raw ioctl interface so the client
 * binary has no libdrm dependency. Flow: GETRESOURCES -> pick CRTC ->
 * GETCONNECTOR -> pick a mode -> CREATE_DUMB -> MAP_DUMB -> ADDFB ->
 * SETCRTC. Presentation is a memcpy into the dumb buffer. */
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

static const char *drm_candidates[] = {
    "/dev/dri/card0", "/dev/dri/card1", "/dev/dri/card2", NULL
};

static uint32_t pick_mode(struct drm_mode_modeinfo *modes, uint32_t n) {
    uint32_t best = 0;
    for (uint32_t i = 0; i < n; i++) {
        /* prefer 1280x720-ish, then anything progressive */
        if (modes[i].hdisplay == 1280 && modes[i].vdisplay == 720) return i;
        if (modes[i].hdisplay > modes[best].hdisplay) best = i;
    }
    return best;
}

int drm_open(struct display_surface *d) {
    for (int i = 0; drm_candidates[i]; i++) {
        int fd = open(drm_candidates[i], O_RDWR | O_CLOEXEC);
        if (fd < 0) continue;

        struct drm_mode_card_res res;
        memset(&res, 0, sizeof res);
        if (ioctl(fd, DRM_IOCTL_MODE_GETRESOURCES, &res) != 0) {
            close(fd); continue;
        }
        if (res.count_connectors == 0 || res.count_crtcs == 0) {
            close(fd); continue;
        }

        uint32_t *conn_ids = calloc(res.count_connectors, sizeof(uint32_t));
        uint32_t *crtc_ids = calloc(res.count_crtcs, sizeof(uint32_t));
        uint32_t *enc_ids  = calloc(res.count_encoders ? res.count_encoders : 1, sizeof(uint32_t));
        uint32_t *fb_ids   = calloc(res.count_fbs ? res.count_fbs : 1, sizeof(uint32_t));
        if (!conn_ids || !crtc_ids || !enc_ids || !fb_ids) {
            free(conn_ids); free(crtc_ids); free(enc_ids); free(fb_ids);
            close(fd); continue;
        }

        struct drm_mode_card_res r2 = res;
        r2.fb_id_ptr        = (uint64_t)(uintptr_t)fb_ids;
        r2.crtc_id_ptr      = (uint64_t)(uintptr_t)crtc_ids;
        r2.connector_id_ptr = (uint64_t)(uintptr_t)conn_ids;
        r2.encoder_id_ptr   = (uint64_t)(uintptr_t)enc_ids;
        if (ioctl(fd, DRM_IOCTL_MODE_GETRESOURCES, &r2) != 0) {
            free(conn_ids); free(crtc_ids); free(enc_ids); free(fb_ids);
            close(fd); continue;
        }

        int found = -1;
        for (uint32_t c = 0; c < res.count_connectors && found < 0; c++) {
            struct drm_mode_get_connector conn;
            memset(&conn, 0, sizeof conn);
            conn.connector_id = conn_ids[c];
            if (ioctl(fd, DRM_IOCTL_MODE_GETCONNECTOR, &conn) != 0) continue;
            if (conn.connection != DRM_MODE_CONNECTED || conn.count_modes == 0) continue;

            struct drm_mode_modeinfo *modes =
                calloc(conn.count_modes, sizeof(struct drm_mode_modeinfo));
            uint32_t *cprops = calloc(conn.count_props ? conn.count_props : 1, sizeof(uint32_t));
            uint64_t *cvals  = calloc(conn.count_props ? conn.count_props : 1, sizeof(uint64_t));
            uint32_t *cenc   = calloc(conn.count_encoders ? conn.count_encoders : 1, sizeof(uint32_t));
            if (!modes || !cprops || !cvals || !cenc) {
                free(modes); free(cprops); free(cvals); free(cenc); continue;
            }
            conn.modes_ptr = (uint64_t)(uintptr_t)modes;
            conn.props_ptr = (uint64_t)(uintptr_t)cprops;
            conn.prop_values_ptr = (uint64_t)(uintptr_t)cvals;
            conn.encoders_ptr = (uint64_t)(uintptr_t)cenc;
            if (ioctl(fd, DRM_IOCTL_MODE_GETCONNECTOR, &conn) != 0) {
                free(modes); free(cprops); free(cvals); free(cenc); continue;
            }
            uint32_t midx = pick_mode(modes, conn.count_modes);
            struct drm_mode_modeinfo chosen = modes[midx];
            uint32_t encoder_id = conn.encoder_id ? conn.encoder_id : cenc[0];

            free(modes); free(cprops); free(cvals); free(cenc);

            /* which CRTC drives that encoder? we just take crtc 0 if the
             * kernel doesn't tell us; a single-panel device almost always
             * has exactly one. */
            uint32_t crtc_id = crtc_ids[0];
            (void)encoder_id;

            struct drm_mode_create_dumb cd;
            memset(&cd, 0, sizeof cd);
            cd.width  = chosen.hdisplay;
            cd.height = chosen.vdisplay;
            cd.bpp    = 32;
            if (ioctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &cd) != 0) continue;

            struct drm_mode_fb_cmd fc;
            memset(&fc, 0, sizeof fc);
            fc.width  = cd.width;
            fc.height = cd.height;
            fc.pitch  = cd.pitch;
            fc.bpp    = cd.bpp;
            fc.depth  = 24;
            fc.handle = cd.handle;
            if (ioctl(fd, DRM_IOCTL_MODE_ADDFB, &fc) != 0) {
                struct drm_mode_destroy_dumb dd; dd.handle = cd.handle;
                ioctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dd);
                continue;
            }

            struct drm_mode_map_dumb md;
            memset(&md, 0, sizeof md);
            md.handle = cd.handle;
            if (ioctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &md) != 0) continue;

            void *map = mmap(NULL, cd.size, PROT_READ | PROT_WRITE, MAP_SHARED,
                             fd, (off_t)md.offset);
            if (map == MAP_FAILED) continue;

            struct drm_mode_crtc crtc;
            memset(&crtc, 0, sizeof crtc);
            crtc.crtc_id = crtc_id;
            crtc.fb_id = fc.fb_id;
            crtc.set_connectors_ptr = (uint64_t)(uintptr_t)&conn_ids[c];
            crtc.count_connectors = 1;
            crtc.mode = chosen;
            crtc.mode_valid = 1;
            if (ioctl(fd, DRM_IOCTL_MODE_SETCRTC, &crtc) != 0) {
                LOGD("SETCRTC failed: %s", strerror(errno));
            }

            memset(d, 0, sizeof *d);
            d->kind = 2;
            d->drm_fd = fd;
            d->fd = fd;
            d->map = map;
            d->map_len = cd.size;
            d->width = cd.width;
            d->height = cd.height;
            d->stride = cd.pitch;
            d->bpp = 32;
            d->pixfmt = PF_XRGB8888;
            d->drm_crtc = crtc_id;
            d->drm_conn = conn_ids[c];
            d->drm_fb = fc.fb_id;
            d->drm_dumb_handle = cd.handle;
            d->drm_w = cd.width; d->drm_h = cd.height; d->drm_pitch = cd.pitch;
            d->active = 1;

            LOGI("display: drm %s %ux%u pitch=%u", drm_candidates[i],
                 d->width, d->height, d->stride);
            found = 0;
        }

        free(conn_ids); free(crtc_ids); free(enc_ids); free(fb_ids);
        if (found == 0) return MRCA_OK;
        close(fd);
    }
    return MRCA_ERR_IO;
}
