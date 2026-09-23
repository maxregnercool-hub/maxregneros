#ifndef MRCA_UTIL_H
#define MRCA_UTIL_H
#include "mrca.h"
#include <stdint.h>
#include <stddef.h>

int64_t  mrca_now_ms(void);
int64_t  mrca_now_us(void);
void     mrca_sleep_ms(int64_t ms);
uint32_t mrca_rand32(void);
void     mrca_rand_seed(uint64_t s);
void     mrca_gen_token(char *out, size_t n);
int      mrca_read_file(const char *path, char *buf, size_t cap);
int      mrca_write_file(const char *path, const char *data);
char    *mrca_trim(char *s);
int      mrca_streq(const char *a, const char *b);
uint32_t mrca_crc32(const void *buf, size_t len);
int      mrca_parse_int(const char *s, int defv);
void     mrca_clamp_rect(int32_t *x, int32_t *y, int32_t *w, int32_t *h,
                         int32_t sw, int32_t sh);
void     mrca_rect_union(int32_t *ax,int32_t *ay,int32_t *aw,int32_t *ah,
                         int32_t  bx,int32_t  by,int32_t  bw,int32_t  bh);

#endif
