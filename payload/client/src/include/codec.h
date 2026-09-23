#ifndef MRCA_CODEC_H
#define MRCA_CODEC_H
#include "mrca.h"

enum { CODEC_RAW = 0, CODEC_JPEG = 1, CODEC_H264 = 2, CODEC_AV1 = 3 };

struct codec_frame {
    const uint8_t *payload;
    size_t         len;
    uint8_t        codec;
    uint8_t        pixfmt;
    uint32_t       width, height, stride;
    struct mrca_rect rects[MRCA_MAX_DIRTY_RECTS];
    uint32_t       nrects;
};

struct display_surface;

int  codec_decode_into(struct display_surface *disp, const struct codec_frame *f);
int  codec_pixfmt_from_str(const char *s);
const char *codec_pixfmt_name(uint8_t pf);
size_t codec_expected_payload(uint32_t w, uint32_t h, uint8_t pixfmt);

#endif
