#include "log.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static FILE *g_fp = NULL;
static int   g_level = LOG_INFO;

void mrca_log_init(int level, const char *path) {
    g_level = level;
    if (path && *path) {
        g_fp = fopen(path, "a");
        if (!g_fp) g_fp = NULL;
    }
}

void mrca_log_close(void) {
    if (g_fp) { fflush(g_fp); fclose(g_fp); g_fp = NULL; }
}

void mrca_log_set_level(int level) { g_level = level; }
int  mrca_log_level(void) { return g_level; }

void mrca_logf(int level, const char *fmt, ...) {
    if (level > g_level) return;
    char ts[32];
    int64_t ms = mrca_now_ms();
    time_t secs = (time_t)(ms / 1000);
    struct tm tmv;
    localtime_r(&secs, &tmv);
    snprintf(ts, sizeof ts, "%02d:%02d:%02d.%03d",
             tmv.tm_hour, tmv.tm_min, tmv.tm_sec, (int)(ms % 1000));

    va_list ap;
    va_start(ap, fmt);
    char line[1024];
    vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);

    fprintf(stderr, "%s %s\n", ts, line);
    if (g_fp) { fprintf(g_fp, "%s %s\n", ts, line); fflush(g_fp); }
}

void mrca_hexdump(int level, const char *tag, const void *buf, size_t len) {
    if (level > g_level) return;
    const unsigned char *p = (const unsigned char *)buf;
    char line[128];
    for (size_t i = 0; i < len; i += 16) {
        size_t n = (len - i < 16) ? (len - i) : 16;
        int off = snprintf(line, sizeof line, "%s %04zx  ", tag, i);
        for (size_t j = 0; j < 16; j++) {
            if (j < n) off += snprintf(line+off, sizeof line-off, "%02x ", p[i+j]);
            else       off += snprintf(line+off, sizeof line-off, "   ");
            if (j == 7) off += snprintf(line+off, sizeof line-off, " ");
        }
        off += snprintf(line+off, sizeof line-off, " |");
        for (size_t j = 0; j < n; j++) {
            unsigned char c = p[i+j];
            line[off++] = (c >= 32 && c < 127) ? (char)c : '.';
        }
        line[off++] = '|'; line[off] = 0;
        mrca_logf(level, "%s", line);
    }
}
