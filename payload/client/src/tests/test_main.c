/* A dependency-free test runner. Every suite is a function returning the
 * number of failures; the runner aggregates and exits non-zero on any. */
#include "mrca.h"
#include "util.h"
#include "ring.h"
#include "proto.h"
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_tests = 0, g_fail = 0;

#define CHECK(cond, msg) do {                                                 \
    g_tests++;                                                                \
    if (!(cond)) { g_fail++; printf("  FAIL %s:%d  %s\n",                     \
                                    __FILE__, __LINE__, msg); }               \
} while (0)

static void test_util(void) {
    printf("- util\n");
    CHECK(mrca_crc32("123456789", 9) == 0xCBF43926u, "crc32 known vector");
    char s1[] = "  hello world  \n";
    CHECK(!strcmp(mrca_trim(s1), "hello world"), "trim");
    CHECK(mrca_parse_int("42", -1) == 42, "parse int");
    CHECK(mrca_parse_int("nope", -1) == -1, "parse int fallback");

    int32_t x=-5,y=-5,w=20,h=20;
    mrca_clamp_rect(&x,&y,&w,&h,100,100);
    CHECK(x==0&&y==0&&w==15&&h==15, "clamp negative origin");

    int32_t ax=0,ay=0,aw=10,ah=10;
    mrca_rect_union(&ax,&ay,&aw,&ah, 20,20,10,10);
    CHECK(ax==0&&ay==0&&aw==30&&ah==30, "rect union");

    char t1[16], t2[16];
    mrca_gen_token(t1, sizeof t1); mrca_gen_token(t2, sizeof t2);
    CHECK(strlen(t1)==15, "token length");
    CHECK(strcmp(t1,t2)!=0, "tokens differ");
}

static void test_ring(void) {
    printf("- ring\n");
    struct mrca_ring r;
    CHECK(mrca_ring_init(&r, 64) == 0, "ring init");
    const char *msg = "abcdefghij";
    CHECK(mrca_ring_write(&r, msg, 10) == 10, "ring write");
    CHECK(mrca_ring_avail(&r) == 10, "ring avail");

    char out[16];
    memset(out, 0, sizeof out);
    CHECK(mrca_ring_peek(&r, out, 10) == 10, "ring peek");
    CHECK(!strcmp(out, "abcdefghij"), "peek content");
    CHECK(mrca_ring_avail(&r) == 10, "peek does not consume");
    CHECK(mrca_ring_read(&r, out, 10) == 10, "ring read");
    CHECK(mrca_ring_avail(&r) == 0, "ring drained");

    char big[100];
    memset(big, 'X', sizeof big);
    size_t wrote = mrca_ring_write(&r, big, 100);
    CHECK(wrote == 64, "ring write caps at capacity");

    mrca_ring_reset(&r);
    CHECK(mrca_ring_avail(&r) == 0, "reset");
    mrca_ring_free(&r);
}

static void test_proto(void) {
    printf("- proto\n");
    struct mrca_hello h;
    memset(&h, 0, sizeof h);
    snprintf(h.device_id, sizeof h.device_id, "dev-1");
    snprintf(h.token, sizeof h.token, "tok");
    h.local_w = 1080; h.local_h = 2340; h.proto = MRCA_PROTO_VERSION; h.caps = 0x07;

    uint8_t *pkt = NULL; size_t n = 0;
    CHECK(proto_pack_hello(&h, &pkt, &n) == MRCA_OK, "pack hello");
    CHECK(n > MRCA_HDR_SIZE, "hello has body");
    CHECK(pkt[0]=='M' && pkt[1]=='R', "magic");
    CHECK(pkt[3]==OP_HELLO, "opcode");
    free(pkt);

    struct mrca_event evs[3];
    memset(evs, 0, sizeof evs);
    evs[0].cls = EV_MT_DOWN; evs[0].x = 100; evs[0].y = 200; evs[0].value = 255;
    evs[1].cls = EV_MT_MOVE; evs[1].x = 101; evs[1].y = 201;
    evs[2].cls = EV_KEY;     evs[2].code = 158; evs[2].value = 1;

    pkt = NULL; n = 0;
    CHECK(proto_pack_input(evs, 3, &pkt, &n) == MRCA_OK, "pack input");
    CHECK(n == MRCA_HDR_SIZE + 4 + 3*MRCA_EVENT_WIRE_SIZE, "input packet size");

    uint8_t op, fl; uint32_t seq, plen;
    CHECK(proto_parse_frame_hdr(pkt, n, &op, &fl, &seq, &plen) == MRCA_OK, "parse hdr");
    CHECK(op == OP_INPUT, "input opcode");
    CHECK(fl & MRCA_F_URGENT, "input marked urgent");
    free(pkt);

    const char *body = "session=abc123\ntoken=xyz\nw=1280\nh=720\n"
                       "pixfmt=2\ncodec=0\nmaxframe=1048576\nfps=60\n";
    size_t blen = strlen(body);
    uint8_t *frame = calloc(1, MRCA_HDR_SIZE + blen + 1);
    frame[0]='M'; frame[1]='R'; frame[3]=OP_WELCOME;
    frame[12] = (uint8_t)(blen & 0xff);
    frame[13] = (uint8_t)((blen >> 8) & 0xff);
    memcpy(frame + MRCA_HDR_SIZE, body, blen);

    struct mrca_welcome wl;
    CHECK(proto_parse_welcome(frame, MRCA_HDR_SIZE + blen, &wl) == MRCA_OK, "parse welcome");
    CHECK(!strcmp(wl.session_id, "abc123"), "welcome session");
    CHECK(wl.remote_w == 1280 && wl.remote_h == 720, "welcome dims");
    CHECK(wl.pixfmt == PF_XRGB8888, "welcome pixfmt");
    CHECK(wl.max_frame_bytes == 1048576, "welcome maxframe");
    free(frame);
}

static void test_config(void) {
    printf("- config\n");
    const char *path = "_test_ca.conf";
    struct mrca_config c;
    CHECK(config_load(&c, path) == MRCA_ERR_IO, "missing config uses defaults");
    CHECK(c.control_port == 9470, "default port");
    CHECK(!strcmp(c.host, "127.0.0.1"), "default host");

    snprintf(c.host, sizeof c.host, "avatar.test");
    c.control_port = 1234;
    snprintf(c.device_id, sizeof c.device_id, "dev-42");
    CHECK(config_save(&c, path) == 0, "save config");

    struct mrca_config c2;
    CHECK(config_load(&c2, path) == MRCA_OK, "reload config");
    CHECK(!strcmp(c2.host, "avatar.test"), "roundtrip host");
    CHECK(c2.control_port == 1234, "roundtrip port");
    CHECK(!strcmp(c2.device_id, "dev-42"), "roundtrip device id");
    remove(path);
}

static void test_status(void) {
    printf("- status\n");
    CHECK(!strcmp(mrca_status_str(MRCA_OK), "ok"), "ok str");
    CHECK(!strcmp(mrca_status_str(MRCA_ERR_AUTH), "auth rejected"), "auth str");
    CHECK(!strcmp(mrca_op_str(OP_FRAME), "FRAME"), "frame op str");
    CHECK(!strcmp(mrca_op_str(0x77), "?"), "unknown op str");
}

int main(void) {
    printf("mrca-client test suite\n");
    test_util();
    test_ring();
    test_proto();
    test_config();
    test_status();
    printf("\n%d checks, %d failures\n", g_tests, g_fail);
    return g_fail ? 1 : 0;
}
