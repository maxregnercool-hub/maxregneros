#ifndef MRCA_WATCHDOG_H
#define MRCA_WATCHDOG_H

#include "mrca.h"
/* A pthread-based liveness watchdog: the session loop pets it every frame; if
 * it starves for longer than the deadline the process is intentionally torn
 * down so init respawns us rather than leaving a wedged client on screen. */
struct watchdog {
    volatile int  running;
    volatile int  tripped;
    volatile long last_pet_ms;
    long          deadline_ms;
    void        (*on_trip)(void);
};

int  watchdog_start(struct watchdog *w, long deadline_ms, void (*on_trip)(void));
void watchdog_pet(struct watchdog *w);
void watchdog_stop(struct watchdog *w);

#endif
