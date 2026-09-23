/* codec.c — payload decode.
 *
 * The default wire codec is CODEC_RAW: the server ships a dirty-rect payload
 * of packed pixels and the client memcpys it into scanout. That is the only
 * configuration in which "no local processing lag" is literally true, and it
 * needs no decoder in the base image.
 *
 * JPEG is available as an opt-in so a low-bandwidth link can trade client CPU
 * for wire bytes; it requires the build to have been made with -DMRCA_HAVE_JPEG
 * and a libjpeg in the target image.
 */
#include "codec.h"
#include "display.h"
#include "log.h"
#include "util.h"
#include <string.h>
#include <stdlib.h>

size_t codec_expected_payload(uint32_t w, uint32_t h, uint8_t pixfmt) {
    size_t bpp = (pixfmt == PF_RGB565) ? 2 : 4;
    return (size_t)w * h * bpp;
}

int codec_pixfmt_from_str(const char *s) {
    if (!s) return PF_UNKNOWN;
    if (!strcmp(s, "rgb565"))   return PF_RGB565;
    if (!strcmp(s, "xrgb8888")) return PF_XRGB8888;
    if (!strcmp(s, "rgba8888")) return PF_RGBA8888;
    if (!strcmp(s, "nv12"))     return PF_NV12;
    return PF_UNKNOWN;
}

const char *codec_pixfmt_name(uint8_t pf) {
    switch (pf) {
        case PF_RGB565:   return "rgb565";
        case PF_XRGB8888: return "xrgb8888";
        case PF_RGBA8888: return "rgba8888";
        case PF_NV12:     return "nv12";
        default:          return "unknown";
    }
}

/* The frame payload layout for CODEC_RAW is:
 *   [rect_count u16][pad u16] then rect_count * { x,y,w,h : int16 LE }
 *   followed by concatenated pixel rows for each rect, rect by rect.
 */
int codec_decode_into(struct display_surface *disp, const struct codec_frame *f) {
    if (!f || !f->payload || f->len < 4) return MRCA_ERR_PROTO;

    if (f->codec == CODEC_RAW) {
        const uint8_t *p = f->payload;
        uint16_t nrects = (uint16_t)(p[0] | (p[1] << 8));
        p += 4;
        if (nrects > MRCA_MAX_DIRTY_RECTS) {
            LOGI("rect count %u exceeds cap", nrects);
            return MRCA_ERR_PROTO;
        }
        size_t hdr = (size_t)nrects * 8;
        if (f->len < 4 + hdr) return MRCA_ERR_PROTO;
        const uint8_t *pix = f->payload + 4 + hdr;
        size_t remain = f->len - 4 - hdr;
        size_t bpp = (f->pixfmt == PF_RGB565) ? 2 : 4;

        for (uint16_t i = 0; i < nrects; i++) {
            const uint8_t *r = f->payload + 4 + (size_t)i * 8;
            int16_t x = (int16_t)(r[0] | (r[1] << 8));
            int16_t y = (int16_t)(r[2] | (r[3] << 8));
            int16_t ww = (int16_t)(r[4] | (r[5] << 8));
            int16_t hh = (int16_t)(r[6] | (r[7] << 8));
            if (ww <= 0 || hh <= 0) continue;
            size_t need = (size_t)ww * hh * bpp;
            if (need > remain) {
                LOGD("truncated rect payload (%zu need, %zu left)", need, remain);
                return MRCA_ERR_PROTO;
            }
            struct mrca_rect rr = { x, y, ww, hh };
            int rc = display_present_rect(disp, pix, f->pixfmt, &rr, (uint32_t)ww * bpp);
            if (rc != MRCA_OK) return rc;
            pix += need;
            remain -= need;
        }
        return MRCA_OK;
    }

#ifdef MRCA_HAVE_JPEG
    if (f->codec == CODEC_JPEG) {
        extern int jpeg_decode_rgba(const uint8_t *in, size_t len,
                                    uint8_t **out, uint32_t *w, uint32_t *h);
        uint8_t *rgba = NULL; uint32_t jw = 0, jh = 0;
        int rc = jpeg_decode_rgba(f->payload, f->len, &rgba, &jw, &jh);
        if (rc != MRCA_OK) return rc;
        struct mrca_rect r = { 0, 0, (int32_t)jw, (int32_t)jh };
        rc = display_present_rect(disp, rgba, PF_RGBA8888, &r, jw * 4);
        free(rgba);
        return rc;
    }
#endif

    LOGI("codec %u not built into this client", f->codec);
    return MRCA_ERR_UNSUPPORTED;
}
