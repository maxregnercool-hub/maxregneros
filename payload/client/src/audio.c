/* audio.c — optional audio sink. The base image is expected to expose a raw
 * PCM device via tinyalsa (/dev/snd/pcmC0D0p). If it doesn't, we run silent
 * rather than failing the session: audio is a capability flag, not a
 * requirement. */
#include "audio.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

int audio_init(struct audio_ctx *a, const char *card) {
    memset(a, 0, sizeof *a);
    a->fd = -1;
    a->sample_rate = 48000;
    a->channels = 2;
    a->bits = 16;

    const char *path = card ? card : "/dev/snd/pcmC0D0p";
    int fd = open(path, O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        LOGD("audio sink %s unavailable (%s): running silent", path, strerror(errno));
        return MRCA_ERR_UNSUPPORTED;
    }
    a->fd = fd;
    a->enabled = 1;
    LOGI("audio: sink %s @ %uHz/%uch/%ubit", path, a->sample_rate, a->channels, a->bits);
    return MRCA_OK;
}

int audio_submit(struct audio_ctx *a, const void *pcm, size_t len) {
    if (!a->enabled || a->fd < 0) return MRCA_ERR_UNSUPPORTED;
    const uint8_t *p = (const uint8_t *)pcm;
    size_t sent = 0;
    while (sent < len) {
        ssize_t r = write(a->fd, p + sent, len - sent);
        if (r > 0) { sent += (size_t)r; continue; }
        if (r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            a->underruns++;
            return (int)sent;
        }
        if (r < 0 && errno == EINTR) continue;
        return -1;
    }
    a->frames_played += len / (a->channels * (a->bits / 8));
    return (int)sent;
}

void audio_shutdown(struct audio_ctx *a) {
    if (a->fd >= 0) { close(a->fd); a->fd = -1; }
    a->enabled = 0;
}
