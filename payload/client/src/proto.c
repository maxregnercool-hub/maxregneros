#include "proto.h"
#include "log.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static void put_u16(uint8_t **p, uint16_t v) {
    (*p)[0]=(uint8_t)(v>>8); (*p)[1]=(uint8_t)(v&0xFF); *p += 2;
}
static void put_u32(uint8_t **p, uint32_t v) {
    (*p)[0]=(uint8_t)(v>>24); (*p)[1]=(uint8_t)(v>>16);
    (*p)[2]=(uint8_t)(v>>8);  (*p)[3]=(uint8_t)(v&0xFF); *p += 4;
}
static int alloc_frame(uint8_t op, uint8_t flags, uint32_t plen,
                       uint8_t **out, size_t *tot) {
    *tot = MRCA_HDR_SIZE + plen;
    uint8_t *b = malloc(*tot);
    if (!b) return MRCA_ERR_NOMEM;
    struct mrca_hdr h;
    memset(&h, 0, sizeof h);
    h.magic[0]='M'; h.magic[1]='R';
    h.version = MRCA_PROTO_VERSION;
    h.op = op; h.flags = flags; h.length = plen;
    mrca_hdr_pack(&h, b);
    *out = b;
    return MRCA_OK;
}

int proto_pack_hello(const struct mrca_hello *h, uint8_t **out, size_t *len) {
    /* JSON body: keeps the control channel debuggable with nothing but nc. */
    char body[1024];
    int n = snprintf(body, sizeof body,
        "{\"device_id\":\"%s\",\"token\":\"%s\",\"w\":%u,\"h\":%u,\"dpi\":%u,"
        "\"proto\":%u,\"caps\":%u,\"model\":\"%s\"}",
        h->device_id, h->token, h->local_w, h->local_h, h->density_dpi,
        h->proto, h->caps, h->model);
    if (n < 0) return MRCA_ERR_GENERIC;
    uint8_t *b = NULL; size_t tot = 0;
    int rc = alloc_frame(OP_HELLO, 0, (uint32_t)n, &b, &tot);
    if (rc != MRCA_OK) return rc;
    memcpy(b + MRCA_HDR_SIZE, body, n);
    *out = b; *len = tot;
    return MRCA_OK;
}

int proto_parse_welcome(const uint8_t *buf, size_t len, struct mrca_welcome *out) {
    if (len < MRCA_HDR_SIZE) return MRCA_ERR_PROTO;
    if (buf[0] != 'M' || buf[1] != 'R') return MRCA_ERR_PROTO;
    if (buf[3] != OP_WELCOME) return MRCA_ERR_PROTO;
    const uint8_t *p = buf + MRCA_HDR_SIZE;
    size_t blen = len - MRCA_HDR_SIZE;
    memset(out, 0, sizeof *out);
    /* body is a compact key:value blob delimited by '\n' */
    char tmp[1024];
    size_t n = blen < sizeof tmp - 1 ? blen : sizeof tmp - 1;
    memcpy(tmp, p, n); tmp[n] = 0;
    char *save = NULL;
    for (char *line = strtok_r(tmp, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        const char *k = line, *v = eq + 1;
        if (!strcmp(k, "session"))       snprintf(out->session_id, sizeof out->session_id, "%s", v);
        else if (!strcmp(k, "token"))    snprintf(out->token, sizeof out->token, "%s", v);
        else if (!strcmp(k, "w"))        out->remote_w = (uint16_t)atoi(v);
        else if (!strcmp(k, "h"))        out->remote_h = (uint16_t)atoi(v);
        else if (!strcmp(k, "pixfmt"))   out->pixfmt = (uint8_t)atoi(v);
        else if (!strcmp(k, "codec"))    out->codec = (uint8_t)atoi(v);
        else if (!strcmp(k, "maxframe")) out->max_frame_bytes = (uint32_t)strtoul(v, NULL, 10);
        else if (!strcmp(k, "fps"))      out->max_fps = (uint16_t)atoi(v);
    }
    if (!out->remote_w || !out->remote_h) return MRCA_ERR_PROTO;
    return MRCA_OK;
}

int proto_pack_input(const struct mrca_event *ev, int n, uint8_t **out, size_t *len) {
    if (n <= 0) return MRCA_ERR_GENERIC;
    /* Binary: [count u8][pad u8][pad u16] then MRCA_EVENT_WIRE_SIZE per event.
     * Each event is cls(1)+slot(1)+code(2)+x(4)+y(4)+value(4)+ts(4) = 20 bytes;
     * allocating 16 here wrote 4 bytes past the end of the buffer. */
    uint32_t plen = 4 + (uint32_t)n * (uint32_t)MRCA_EVENT_WIRE_SIZE;
    uint8_t *b = NULL; size_t tot = 0;
    int rc = alloc_frame(OP_INPUT, MRCA_F_URGENT, plen, &b, &tot);
    if (rc != MRCA_OK) return rc;
    uint8_t *p = b + MRCA_HDR_SIZE;
    p[0] = (uint8_t)n; p[1] = 0; p[2] = 0; p[3] = 0; p += 4;
    for (int i = 0; i < n; i++) {
        const struct mrca_event *e = &ev[i];
        p[0] = e->cls;
        p[1] = e->slot;
        p += 2;                 /* step past cls+slot or put_u16 clobbers them */
        put_u16(&p, e->code);
        put_u32(&p, (uint32_t)e->x);
        put_u32(&p, (uint32_t)e->y);
        put_u32(&p, (uint32_t)e->value);
        put_u32(&p, (uint32_t)e->ts_ms);
    }
    *out = b; *len = tot;
    return MRCA_OK;
}

int proto_pack_stat(const char *json, uint8_t **out, size_t *len) {
    size_t n = strlen(json);
    uint8_t *b = NULL; size_t tot = 0;
    int rc = alloc_frame(OP_STAT, 0, (uint32_t)n, &b, &tot);
    if (rc != MRCA_OK) return rc;
    memcpy(b + MRCA_HDR_SIZE, json, n);
    *out = b; *len = tot;
    return MRCA_OK;
}

int proto_parse_frame_hdr(const uint8_t *buf, size_t len, uint8_t *op, uint8_t *flags,
                          uint32_t *seq, uint32_t *plen) {
    if (len < MRCA_HDR_SIZE) return MRCA_ERR_PROTO;
    if (buf[0] != 'M' || buf[1] != 'R') return MRCA_ERR_PROTO;
    *op    = buf[3];
    *flags = buf[4];
    /* BIG-ENDIAN - must match mrca_hdr_pack() and the server's HDR_STRUCT. */
    *seq   = ((uint32_t)buf[8] << 24) | ((uint32_t)buf[9] << 16) |
             ((uint32_t)buf[10] << 8) |  (uint32_t)buf[11];
    *plen  = ((uint32_t)buf[12] << 24)| ((uint32_t)buf[13] << 16)|
             ((uint32_t)buf[14] << 8) |  (uint32_t)buf[15];
    return MRCA_OK;
}
