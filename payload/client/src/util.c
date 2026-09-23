#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

static uint64_t  g_rng = 0x9E3779B97F4A7C15ull;

int64_t mrca_now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}
int64_t mrca_now_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}
void mrca_sleep_ms(int64_t ms) {
    if (ms <= 0) return;
    struct timespec ts = { ms/1000, (ms%1000)*1000000 };
    nanosleep(&ts, NULL);
}
void mrca_rand_seed(uint64_t s) { g_rng = s ? s : 1; }
uint32_t mrca_rand32(void) {
    /* xorshift64* */
    g_rng ^= g_rng >> 12; g_rng ^= g_rng << 25; g_rng ^= g_rng >> 27;
    return (uint32_t)((g_rng * 0x2545F4914F6CDD1Dull) >> 32);
}
void mrca_gen_token(char *out, size_t n) {
    static const char *hexd = "0123456789abcdef";
    if (n < 2) { if (n) out[0]=0; return; }
    for (size_t i = 0; i + 1 < n; i++) out[i] = hexd[mrca_rand32() & 0xF];
    out[n-1] = 0;
}
int mrca_read_file(const char *path, char *buf, size_t cap) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return -1;
    size_t n = fread(buf, 1, cap-1, fp);
    fclose(fp);
    buf[n] = 0;
    return (int)n;
}
int mrca_write_file(const char *path, const char *data) {
    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;
    size_t n = strlen(data);
    size_t wr = fwrite(data, 1, n, fp);
    fclose(fp);
    return wr == n ? 0 : -1;
}
char *mrca_trim(char *s) {
    if (!s) return s;
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
    char *e = s + strlen(s);
    while (e > s && (e[-1]==' '||e[-1]=='\t'||e[-1]=='\r'||e[-1]=='\n')) *--e = 0;
    return s;
}
int mrca_streq(const char *a, const char *b) {
    if (!a || !b) return 0;
    return strcmp(a,b) == 0;
}
uint32_t mrca_crc32(const void *buf, size_t len) {
    static uint32_t tbl[256];
    static int init = 0;
    if (!init) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            tbl[i] = c;
        }
        init = 1;
    }
    const unsigned char *p = (const unsigned char *)buf;
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) crc = tbl[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}
int mrca_parse_int(const char *s, int defv) {
    if (!s || !*s) return defv;
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (end == s) return defv;
    return (int)v;
}
void mrca_clamp_rect(int32_t *x, int32_t *y, int32_t *w, int32_t *h,
                     int32_t sw, int32_t sh) {
    if (*x < 0) { *w += *x; *x = 0; }
    if (*y < 0) { *h += *y; *y = 0; }
    if (*x + *w > sw) *w = sw - *x;
    if (*y + *h > sh) *h = sh - *y;
    if (*w < 0) *w = 0;
    if (*h < 0) *h = 0;
}
void mrca_rect_union(int32_t *ax,int32_t *ay,int32_t *aw,int32_t *ah,
                     int32_t  bx,int32_t  by,int32_t  bw,int32_t  bh) {
    int32_t x1 = *ax, y1 = *ay, x2 = *ax + *aw, y2 = *ay + *ah;
    int32_t ux1 = bx,  uy1 = by,  ux2 = bx + bw,  uy2 = by + bh;
    int32_t nx1 = x1 < ux1 ? x1 : ux1, ny1 = y1 < uy1 ? y1 : uy1;
    int32_t nx2 = x2 > ux2 ? x2 : ux2, ny2 = y2 > uy2 ? y2 : uy2;
    *ax = nx1; *ay = ny1; *aw = nx2 - nx1; *ah = ny2 - ny1;
}
