/* mrca.h — MaxRegnerOS Cloud Avatar client, core definitions.
 * Everything in this tree is plain C11, no external dependencies beyond
 * libc/pthread. The daemon is designed to run as root on an Android base
 * system that has had SurfaceFlinger, the Java runtime and the app
 * framework stripped out.
 */
#ifndef MRCA_H
#define MRCA_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define MRCA_VERSION_MAJOR 1
#define MRCA_VERSION_MINOR 0
#define MRCA_VERSION_PATCH 0
#define MRCA_PROTO_VERSION 3

#define MRCA_MAX_EVENTS      64
#define MRCA_MAX_DIRTY_RECTS 128
#define MRCA_EVENT_WIRE_SIZE 20u
#define MRCA_MAX_FRAME_BYTES (16u * 1024u * 1024u)
#define MRCA_MAX_HANDSHAKE   (64u * 1024u)

/* ── error codes ─────────────────────────────────────────────── */
enum mrca_status {
    MRCA_OK              =  0,
    MRCA_ERR_GENERIC     = -1,
    MRCA_ERR_NOMEM       = -2,
    MRCA_ERR_IO          = -3,
    MRCA_ERR_PROTO       = -4,
    MRCA_ERR_AUTH        = -5,
    MRCA_ERR_TIMEOUT     = -6,
    MRCA_ERR_UNSUPPORTED = -7,
    MRCA_ERR_AGAIN       = -8,
    MRCA_ERR_STATE       = -9,
};

/* ── wire opcodes ────────────────────────────────────────────── */
enum mrca_op {
    OP_HELLO      = 0x01,
    OP_WELCOME    = 0x02,
    OP_INPUT      = 0x03,
    OP_FRAME      = 0x04,
    OP_STAT       = 0x05,
    OP_QUALITY    = 0x06,
    OP_PING       = 0x07,
    OP_PONG       = 0x08,
    OP_BYE        = 0x09,
    OP_AUDIO      = 0x0A,
    OP_KEYFRAME   = 0x0B,
    OP_CLIPBOARD  = 0x0C,
    OP_SCROLL_HINT= 0x0D,
};

/* flags */
#define MRCA_F_ACK       0x01
#define MRCA_F_URGENT    0x02
#define MRCA_F_KEYFRAME  0x04
#define MRCA_F_FRAGMENT  0x08
#define MRCA_F_LAST_FRAG 0x10

/* pixel formats, kept numerically identical to the server's enum */
enum mrca_pixfmt {
    PF_UNKNOWN = 0,
    PF_RGB565  = 1,
    PF_XRGB8888= 2,
    PF_RGBA8888= 3,
    PF_NV12    = 4,
};

/* input event classes */
enum mrca_evclass {
    EV_MT_DOWN = 1,
    EV_MT_UP   = 2,
    EV_MT_MOVE = 3,
    EV_KEY     = 4,
    EV_SCROLL  = 5,
    EV_ORIENT  = 6,
};

struct mrca_rect { int32_t x, y, w, h; };

struct mrca_event {
    uint8_t  cls;
    uint8_t  slot;      /* multitouch slot index */
    uint16_t code;      /* keycode / axis code */
    int32_t  x, y;      /* remote-space coordinates */
    int32_t  value;     /* pressure, scroll delta, key state */
    uint32_t ts_ms;
};

/* 16-byte frame header shared by every opcode */
struct mrca_hdr {
    uint8_t  magic[2];
    uint8_t  version;
    uint8_t  op;
    uint8_t  flags;
    uint8_t  reserved[3];
    uint32_t seq;
    uint32_t length;
};

#define MRCA_HDR_SIZE 16
static inline void mrca_hdr_pack(const struct mrca_hdr *h, uint8_t out[MRCA_HDR_SIZE]) {
    out[0]='M'; out[1]='R'; out[2]=h->version; out[3]=h->op; out[4]=h->flags;
    out[5]=out[6]=out[7]=0;
    /* BIG-ENDIAN, matching mrca_server.protocol.HDR_STRUCT (">2sBBB3xII"). */
    out[8]  = (uint8_t)((h->seq >> 24) & 0xff);
    out[9]  = (uint8_t)((h->seq >> 16) & 0xff);
    out[10] = (uint8_t)((h->seq >> 8) & 0xff);
    out[11] = (uint8_t)(h->seq & 0xff);
    out[12] = (uint8_t)((h->length >> 24) & 0xff);
    out[13] = (uint8_t)((h->length >> 16) & 0xff);
    out[14] = (uint8_t)((h->length >> 8) & 0xff);
    out[15] = (uint8_t)(h->length & 0xff);
}

const char *mrca_status_str(int st);
const char *mrca_op_str(int op);

#endif /* MRCA_H */
