#ifndef MRCA_AUDIO_H
#define MRCA_AUDIO_H
#include "mrca.h"
#include <stddef.h>
#include <stdint.h>

struct audio_ctx {
    int      enabled;
    int      fd;
    uint32_t sample_rate;
    uint8_t  channels;
    uint8_t  bits;
    uint64_t frames_played;
    uint64_t underruns;
};

int  audio_init(struct audio_ctx *a, const char *card);
void audio_shutdown(struct audio_ctx *a);
int  audio_submit(struct audio_ctx *a, const void *pcm, size_t len);

#endif
