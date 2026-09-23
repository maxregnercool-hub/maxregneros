#include "ws.h"
#include "log.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <time.h>
#include <poll.h>

/* A minimal but correct RFC6455 client. Server frames are accepted unmasked;
 * client frames are always masked, as the spec requires. */

#define WS_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

static const char b64tab[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

void mrca_b64(const uint8_t *in, size_t n, char *out, size_t outcap) {
    size_t o = 0;
    for (size_t i = 0; i < n; i += 3) {
        uint32_t v = in[i] << 16;
        if (i+1 < n) v |= in[i+1] << 8;
        if (i+2 < n) v |= in[i+2];
        if (o + 4 >= outcap) break;
        out[o++] = b64tab[(v >> 18) & 63];
        out[o++] = b64tab[(v >> 12) & 63];
        out[o++] = (i+1 < n) ? b64tab[(v >> 6) & 63] : '=';
        out[o++] = (i+2 < n) ? b64tab[v & 63]        : '=';
    }
    out[o] = 0;
}

/* SHA-1, needed only for the handshake Accept header. */
struct sha1_ctx { uint32_t h[5]; uint64_t len; uint8_t buf[64]; size_t idx; };

static void sha1_block(struct sha1_ctx *c, const uint8_t *p) {
    uint32_t w[80];
    for (int i = 0; i < 16; i++)
        w[i] = (p[i*4]<<24)|(p[i*4+1]<<16)|(p[i*4+2]<<8)|p[i*4+3];
    for (int i = 16; i < 80; i++) {
        uint32_t v = w[i-3]^w[i-8]^w[i-14]^w[i-16];
        w[i] = (v<<1)|(v>>31);
    }
    uint32_t a=c->h[0],b=c->h[1],d=c->h[2],e=c->h[3],f=c->h[4];
    for (int i = 0; i < 80; i++) {
        uint32_t t, k;
        if (i < 20)      { t = (b&d)|((~b)&e);       k = 0x5A827999; }
        else if (i < 40) { t = b^d^e;                k = 0x6ED9EBA1; }
        else if (i < 60) { t = (b&d)|(b&e)|(d&e);    k = 0x8F1BBCDC; }
        else             { t = b^d^e;                k = 0xCA62C1D6; }
        uint32_t tmp = ((a<<5)|(a>>27)) + t + f + k + w[i];
        f = e; e = d; d = (b<<30)|(b>>2); b = a; a = tmp;
    }
    c->h[0]+=a; c->h[1]+=b; c->h[2]+=d; c->h[3]+=e; c->h[4]+=f;
}
static void sha1_init(struct sha1_ctx *c) {
    c->h[0]=0x67452301; c->h[1]=0xEFCDAB89; c->h[2]=0x98BADCFE;
    c->h[3]=0x10325476; c->h[4]=0xC3D2E1F0; c->len=0; c->idx=0;
}
static void sha1_update(struct sha1_ctx *c, const void *data, size_t n) {
    const uint8_t *p = (const uint8_t *)data;
    c->len += n;
    while (n) {
        size_t take = 64 - c->idx;
        if (take > n) take = n;
        memcpy(c->buf + c->idx, p, take);
        c->idx += take; p += take; n -= take;
        if (c->idx == 64) { sha1_block(c, c->buf); c->idx = 0; }
    }
}
static void sha1_final(struct sha1_ctx *c, uint8_t out[20]) {
    uint64_t bits = c->len * 8;
    uint8_t pad = 0x80;
    sha1_update(c, &pad, 1);
    uint8_t zero = 0;
    while (c->idx != 56) sha1_update(c, &zero, 1);
    uint8_t lenb[8];
    for (int i = 0; i < 8; i++) lenb[i] = (uint8_t)(bits >> (56 - i*8));
    sha1_update(c, lenb, 8);
    for (int i = 0; i < 5; i++) {
        out[i*4+0] = (uint8_t)(c->h[i] >> 24);
        out[i*4+1] = (uint8_t)(c->h[i] >> 16);
        out[i*4+2] = (uint8_t)(c->h[i] >> 8);
        out[i*4+3] = (uint8_t)(c->h[i]);
    }
}

unsigned ws_expected_accept(const char *client_key, char out[29]) {
    char cat[128];
    snprintf(cat, sizeof cat, "%s" WS_GUID, client_key);
    struct sha1_ctx c; sha1_init(&c);
    sha1_update(&c, cat, strlen(cat));
    uint8_t dig[20]; sha1_final(&c, dig);
    char b64[29];
    mrca_b64(dig, 20, b64, sizeof b64);
    snprintf(out, 29, "%s", b64);
    return (unsigned)strlen(out);
}

int ws_connect(struct ws_conn *w, const char *host, int port, const char *path,
               const char *host_hdr, const char *extra_headers, int timeout_ms) {
    memset(w, 0, sizeof *w);
    int rc = mrca_net_connect(&w->net, host, port, timeout_ms, 1);
    if (rc != MRCA_OK) return rc;
    w->is_client = 1;

    char key[25];
    for (int i = 0; i < 16; i++) key[i] = (char)(mrca_rand32() & 0xFF);
    char keyb64[32];
    mrca_b64((const uint8_t *)key, 16, keyb64, sizeof keyb64);

    char req[1024];
    int n = snprintf(req, sizeof req,
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: %s\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "%s"
        "\r\n",
        path, host_hdr ? host_hdr : host, keyb64,
        extra_headers ? extra_headers : "");
    if (mrca_net_write(&w->net, req, n) != n) {
        mrca_net_close(&w->net);
        return MRCA_ERR_IO;
    }

    char resp[2048];
    size_t got = 0;
    int64_t deadline = mrca_now_ms() + (timeout_ms > 0 ? timeout_ms : 5000);
    while (got < sizeof resp - 1) {
        ssize_t r = mrca_net_read(&w->net, resp + got, 1);
        if (r == 1) {
            got++;
            if (got >= 4 && memcmp(resp + got - 4, "\r\n\r\n", 4) == 0) break;
            continue;
        }
        if (mrca_now_ms() > deadline) { mrca_net_close(&w->net); return MRCA_ERR_TIMEOUT; }
    }
    resp[got] = 0;
    if (!strstr(resp, "101")) {
        LOGI("handshake rejected: %.80s", resp);
        mrca_net_close(&w->net);
        return MRCA_ERR_PROTO;
    }
    char expect[29];
    ws_expected_accept(keyb64, expect);
    if (!strstr(resp, expect)) {
        LOGI("bad Sec-WebSocket-Accept");
        mrca_net_close(&w->net);
        return MRCA_ERR_PROTO;
    }
    return MRCA_OK;
}

static int ws_send_frame(struct ws_conn *w, int opcode, const void *payload, size_t len) {
    uint8_t hdr[14];
    size_t hlen = 0;
    hdr[0] = (uint8_t)(0x80 | (opcode & 0x0F));
    int mask = w->is_client;
    if (len < 126) {
        hdr[1] = (uint8_t)((mask ? 0x80 : 0) | len);
        hlen = 2;
    } else if (len <= 0xFFFF) {
        hdr[1] = (uint8_t)((mask ? 0x80 : 0) | 126);
        hdr[2] = (uint8_t)(len >> 8);
        hdr[3] = (uint8_t)(len & 0xFF);
        hlen = 4;
    } else {
        hdr[1] = (uint8_t)((mask ? 0x80 : 0) | 127);
        for (int i = 0; i < 8; i++) hdr[2+i] = (uint8_t)((uint64_t)len >> (56 - i*8));
        hlen = 10;
    }
    uint8_t maskkey[4] = {0,0,0,0};
    if (mask) {
        uint32_t m = mrca_rand32();
        maskkey[0] = (uint8_t)(m >> 24);
        maskkey[1] = (uint8_t)(m >> 16);
        maskkey[2] = (uint8_t)(m >> 8);
        maskkey[3] = (uint8_t)(m);
        memcpy(hdr + hlen, maskkey, 4);
        hlen += 4;
    }
    if (mrca_net_write(&w->net, hdr, hlen) != (ssize_t)hlen) return MRCA_ERR_IO;
    if (len == 0) return MRCA_OK;
    if (!mask) {
        return mrca_net_write(&w->net, payload, len) == (ssize_t)len ? MRCA_OK : MRCA_ERR_IO;
    }
    uint8_t *tmp = malloc(len);
    if (!tmp) return MRCA_ERR_NOMEM;
    const uint8_t *src = (const uint8_t *)payload;
    for (size_t i = 0; i < len; i++) tmp[i] = src[i] ^ maskkey[i & 3];
    int rc = (mrca_net_write(&w->net, tmp, len) == (ssize_t)len) ? MRCA_OK : MRCA_ERR_IO;
    free(tmp);
    return rc;
}

int ws_send(struct ws_conn *w, int opcode, const void *payload, size_t len) {
    return ws_send_frame(w, opcode, payload, len);
}
int ws_send_text(struct ws_conn *w, const char *s) {
    return ws_send_frame(w, WS_TEXT, s, strlen(s));
}
int ws_ping(struct ws_conn *w, const void *payload, size_t len) {
    return ws_send_frame(w, WS_PING, payload, len);
}

int ws_recv(struct ws_conn *w, int *opcode, uint8_t **payload, size_t *len) {
    return ws_recv_timeout(w, opcode, payload, len, 30000);
}


/* Wait for readability without ever blocking on a blocking socket.
 * Returning 0 here means "nothing pending" - the caller must treat that as a
 * timeout, not as a dead connection. */
static int ws_wait_readable(int fd, int ms) {
    struct pollfd p;
    p.fd = fd; p.events = POLLIN; p.revents = 0;
    int r;
    do { r = poll(&p, 1, ms); } while (r < 0 && errno == EINTR);
    if (r <= 0) return r;                                  /* 0 = timeout */
    if (p.revents & (POLLERR | POLLHUP | POLLNVAL)) return -1;
    return 1;
}

int ws_recv_timeout(struct ws_conn *w, int *opcode, uint8_t **payload,
                    size_t *len, int timeout_ms) {
    *opcode = -1; *payload = NULL; *len = 0;
    int ready = ws_wait_readable(w->net.fd, timeout_ms);
    if (ready == 0) return MRCA_ERR_TIMEOUT;
    if (ready < 0)  return MRCA_ERR_IO;
    uint8_t h2[2];
    {   ssize_t r = mrca_net_read_exact(&w->net, h2, 2, timeout_ms);
        if (r != 2) return (r < 0) ? (int)r : MRCA_ERR_IO; }
    int fin    = h2[0] & 0x80;
    int op     = h2[0] & 0x0F;
    int masked = h2[1] & 0x80;
    uint64_t plen = h2[1] & 0x7F;
    if (plen == 126) {
        uint8_t e[2];
        if (mrca_net_read_exact(&w->net, e, 2, timeout_ms) != 2) return MRCA_ERR_IO;
        plen = ((uint64_t)e[0] << 8) | e[1];
    } else if (plen == 127) {
        uint8_t e[8];
        if (mrca_net_read_exact(&w->net, e, 8, timeout_ms) != 8) return MRCA_ERR_IO;
        plen = 0;
        for (int i = 0; i < 8; i++) plen = (plen << 8) | e[i];
    }
    uint8_t maskkey[4] = {0,0,0,0};
    if (masked) {
        if (mrca_net_read_exact(&w->net, maskkey, 4, timeout_ms) != 4) return MRCA_ERR_IO;
    }
    if (plen > MRCA_MAX_FRAME_BYTES) {
        LOGI("frame too large: %llu", (unsigned long long)plen);
        return MRCA_ERR_PROTO;
    }
    uint8_t *buf = NULL;
    if (plen) {
        buf = malloc(plen ? plen : 1);
        if (!buf) return MRCA_ERR_NOMEM;
        if (mrca_net_read_exact(&w->net, buf, plen, timeout_ms) != (ssize_t)plen) {
            free(buf); return MRCA_ERR_IO;
        }
        if (masked) for (uint64_t i = 0; i < plen; i++) buf[i] ^= maskkey[i & 3];
    }
    if (!fin) {
        LOGI("fragmented frames not expected on the stream channel");
        free(buf);
        return MRCA_ERR_PROTO;
    }
    *opcode = op; *payload = buf; *len = (size_t)plen;
    return MRCA_OK;
}

void ws_close(struct ws_conn *w) {
    if (w->net.fd >= 0) {
        uint8_t code[2] = { 0x03, 0xE8 }; /* 1000 */
        ws_send_frame(w, WS_CLOSE, code, 2);
        mrca_net_close(&w->net);
    }
}
