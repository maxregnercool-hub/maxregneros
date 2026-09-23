#ifndef MRCA_PROTO_H
#define MRCA_PROTO_H
#include "mrca.h"

struct mrca_hello {
    uint32_t device_id_crc;
    char     device_id[64];
    char     token[128];
    uint16_t local_w, local_h;
    uint16_t density_dpi;
    uint16_t proto;
    uint8_t  caps;          /* bitfield */
    char     model[64];
};

struct mrca_welcome {
    char     session_id[64];
    char     token[128];
    uint16_t remote_w, remote_h;
    uint8_t  pixfmt;
    uint8_t  codec;
    uint32_t max_frame_bytes;
    uint16_t max_fps;
};

#define CAP_MT       0x01
#define CAP_KEY      0x02
#define CAP_SCROLL   0x04
#define CAP_AUDIO    0x08
#define CAP_CLIP     0x10

int proto_pack_hello(const struct mrca_hello *h, uint8_t **out, size_t *len);
int proto_parse_welcome(const uint8_t *buf, size_t len, struct mrca_welcome *out);
int proto_pack_input(const struct mrca_event *ev, int n, uint8_t **out, size_t *len);
int proto_pack_stat(const char *json, uint8_t **out, size_t *len);
int proto_parse_frame_hdr(const uint8_t *buf, size_t len, uint8_t *op, uint8_t *flags,
                          uint32_t *seq, uint32_t *plen);

#endif
