#ifndef MRCA_LOG_H
#define MRCA_LOG_H
#include "mrca.h"
#include <stddef.h>
#include <stdarg.h>

enum { LOG_QUIET=0, LOG_INFO=1, LOG_DEBUG=2, LOG_TRACE=3 };

void mrca_log_init(int level, const char *path);
void mrca_log_close(void);
void mrca_log_set_level(int level);
int  mrca_log_level(void);
void mrca_logf(int level, const char *fmt, ...) __attribute__((format(printf,2,3)));
void mrca_hexdump(int level, const char *tag, const void *buf, size_t len);

#define LOGI(...)  mrca_logf(LOG_INFO,  __VA_ARGS__)
#define LOGD(...)  mrca_logf(LOG_DEBUG, __VA_ARGS__)
#define LOGT(...)  mrca_logf(LOG_TRACE, __VA_ARGS__)
#define LOGE(...)  mrca_logf(LOG_INFO,  "[E] " __VA_ARGS__)

#endif
