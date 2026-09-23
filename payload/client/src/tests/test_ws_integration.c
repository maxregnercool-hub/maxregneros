/* Integration test for the control-channel receive path.
 * Exercises the exact functions ctl_drain() depends on, over a real socket:
 *   1. a WS_BIN frame carrying an MR-framed OP_WELCOME decodes correctly
 *   2. an idle socket reports TIMEOUT/AGAIN, NOT an IO error
 *      (this is the property the control drain relies on - misclassifying a
 *       timeout as a dead socket would tear down a healthy session)
 *   3. proto_parse_frame_hdr agrees with the packed header
 */
#include "mrca.h"
#include "ws.h"
#include "proto.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

static int fails = 0;
#define CHECK(cond, msg) do { \
    if (cond) { printf("  ok   %s\n", msg); } \
    else { printf("  FAIL %s\n", msg); fails++; } } while (0)

static void put_frame(uint8_t *out, int opcode, const uint8_t *pl, int n) {
    out[0] = (uint8_t)(0x80 | opcode);   /* FIN + opcode */
    out[1] = (uint8_t)n;                 /* <126, unmasked (server->client) */
    if (n) memcpy(out + 2, pl, (size_t)n);
}

int main(void) {
    printf("mrca control-channel integration suite\n");

    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
        printf("  FAIL socketpair unavailable\n"); return 1;
    }

    struct ws_conn w;
    memset(&w, 0, sizeof w);
    w.net.fd = sv[0];
    w.is_client = 1;

    /* --- build an MR-framed OP_WELCOME and wrap it in a websocket frame --- */
    struct mrca_hdr h;
    memset(&h, 0, sizeof h);
    h.magic[0] = 'M'; h.magic[1] = 'R';
    h.version = MRCA_PROTO_VERSION;
    h.op = OP_WELCOME;
    h.length = 8;

    uint8_t mr[MRCA_HDR_SIZE + 8];
    mrca_hdr_pack(&h, mr);
    for (int i = 0; i < 8; i++) mr[MRCA_HDR_SIZE + i] = (uint8_t)(i + 1);

    uint8_t frame[2 + MRCA_HDR_SIZE + 8];
    put_frame(frame, WS_BIN, mr, (int)sizeof mr);
    CHECK(write(sv[1], frame, sizeof frame) == (ssize_t)sizeof frame, "server frame written");

    /* --- 1. receive + decode --- */
    int op = -1; uint8_t *payload = NULL; size_t len = 0;
    int rc = ws_recv_timeout(&w, &op, &payload, &len, 1000);
    CHECK(rc == MRCA_OK,          "ws_recv_timeout returns MRCA_OK on data");
    CHECK(op == WS_BIN,           "opcode is WS_BIN");
    CHECK(len == sizeof mr,        "payload length matches");

    if (rc == MRCA_OK && payload) {
        uint8_t fop = 0, fflags = 0; uint32_t seq = 0, plen = 0;
        int prc = proto_parse_frame_hdr(payload, len, &fop, &fflags, &seq, &plen);
        CHECK(prc == MRCA_OK,         "proto_parse_frame_hdr accepts the frame");
        CHECK(fop == OP_WELCOME,      "inner opcode resolves to OP_WELCOME");
        CHECK(plen == 8,              "inner length resolves to 8");
        CHECK(payload[MRCA_HDR_SIZE] == 1 && payload[MRCA_HDR_SIZE + 7] == 8,
                                      "payload bytes survive the round trip");
    }

    /* --- 2. idle socket must NOT look like a dead socket --- */
    uint8_t *p2 = NULL; size_t l2 = 0; int o2 = -1;
    int rc2 = ws_recv_timeout(&w, &o2, &p2, &l2, 60);
    CHECK(rc2 == MRCA_ERR_TIMEOUT || rc2 == MRCA_ERR_AGAIN,
          "idle socket reports TIMEOUT/AGAIN (not IO)");
    CHECK(rc2 != MRCA_ERR_IO,     "idle socket is not misread as a broken connection");

    /* --- 3. header pack/parse agreement on a second opcode --- */
    struct mrca_hdr h3; memset(&h3, 0, sizeof h3);
    h3.magic[0]='M'; h3.magic[1]='R'; h3.version=MRCA_PROTO_VERSION;
    h3.op = OP_AUDIO; h3.flags = MRCA_F_ACK; h3.length = 4321;
    uint8_t b3[MRCA_HDR_SIZE]; mrca_hdr_pack(&h3, b3);
    uint8_t fop3=0, ffl3=0; uint32_t sq3=0, pl3=0;
    CHECK(proto_parse_frame_hdr(b3, sizeof b3, &fop3, &ffl3, &sq3, &pl3) == MRCA_OK,
          "packed header re-parses");
    CHECK(fop3 == OP_AUDIO && pl3 == 4321, "opcode + length survive pack/parse");

    free(payload); free(p2);
    close(sv[0]); close(sv[1]);
    printf("  -> %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
