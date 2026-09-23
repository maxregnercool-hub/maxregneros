/* Emits wire vectors produced by the REAL C packing code, so the Python
 * server's protocol module can be checked against the bytes the client
 * actually puts on the wire. Anti-drift: this is the test that would have
 * caught the header endianness and 20-vs-16 event size bugs. */
#include "mrca.h"
#include "proto.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    struct { uint8_t op; uint8_t flags; uint32_t seq; uint32_t len; } hv[] = {
        { OP_WELCOME, 0,               0u,          8u },
        { OP_FRAME,   MRCA_F_KEYFRAME, 1u,     65536u },
        { OP_INPUT,   MRCA_F_URGENT,   0x01020304u, 0x0A0B0C0Du },
        { OP_AUDIO,   0,               0xFFFFFFFFu, 0u },
    };
    for (unsigned i = 0; i < sizeof hv / sizeof hv[0]; i++) {
        struct mrca_hdr h; memset(&h, 0, sizeof h);
        h.magic[0]='M'; h.magic[1]='R'; h.version = MRCA_PROTO_VERSION;
        h.op = hv[i].op; h.flags = hv[i].flags;
        h.seq = hv[i].seq; h.length = hv[i].len;
        uint8_t b[MRCA_HDR_SIZE];
        mrca_hdr_pack(&h, b);
        printf("HDR %u %u %u %u ", hv[i].op, hv[i].flags, hv[i].seq, hv[i].len);
        for (int k = 0; k < MRCA_HDR_SIZE; k++) printf("%02x", b[k]);
        printf("\n");
    }

    struct mrca_event ev[3];
    memset(ev, 0, sizeof ev);
    ev[0].cls=EV_MT_DOWN; ev[0].slot=0; ev[0].code=0x014a;
    ev[0].x=123;  ev[0].y=456; ev[0].value=77; ev[0].ts_ms=1000;
    ev[1].cls=EV_MT_MOVE; ev[1].slot=0; ev[1].code=0x014a;
    ev[1].x=124;  ev[1].y=458; ev[1].value=77; ev[1].ts_ms=1016;
    ev[2].cls=EV_MT_UP;   ev[2].slot=1; ev[2].code=0x001e;
    ev[2].x=124;  ev[2].y=458; ev[2].value=0;  ev[2].ts_ms=1032;

    uint8_t *pkt = NULL; size_t n = 0;
    if (proto_pack_input(ev, 3, &pkt, &n) != MRCA_OK) return 1;
    printf("INPUT %zu ", n);
    for (size_t k = 0; k < n; k++) printf("%02x", pkt[k]);
    printf("\n");
    free(pkt);
    printf("EVENTSIZE %u\n", (unsigned)MRCA_EVENT_WIRE_SIZE);
    return 0;
}
