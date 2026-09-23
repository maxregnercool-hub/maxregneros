#ifndef MRCA_WS_H
#define MRCA_WS_H
#include "mrca.h"
#include "net.h"
#include <stdint.h>
#include <stddef.h>

enum ws_opcode {
    WS_CONT   = 0x0,
    WS_TEXT   = 0x1,
    WS_BIN    = 0x2,
    WS_CLOSE  = 0x8,
    WS_PING   = 0x9,
    WS_PONG   = 0xA,
};

struct ws_conn {
    struct mrca_conn net;
    int              is_client;
    int              closing;
};

int  ws_connect(struct ws_conn *w, const char *host, int port, const char *path,
                const char *host_hdr, const char *extra_headers, int timeout_ms);
int  ws_send(struct ws_conn *w, int opcode, const void *payload, size_t len);
int  ws_send_text(struct ws_conn *w, const char *s);
int  ws_recv(struct ws_conn *w, int *opcode, uint8_t **payload, size_t *len);
int  ws_recv_timeout(struct ws_conn *w, int *opcode, uint8_t **payload,
                     size_t *len, int timeout_ms);
int  ws_ping(struct ws_conn *w, const void *payload, size_t len);
void ws_close(struct ws_conn *w);
unsigned ws_expected_accept(const char *client_key, char out[29]);

#endif
