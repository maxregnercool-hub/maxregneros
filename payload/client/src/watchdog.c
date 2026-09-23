#include "watchdog.h"
#include "util.h"
#include "log.h"
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>

extern int64_t mrca_now_ms(void);

static void *watchdog_thread(void *arg) {
    struct watchdog *w = (struct watchdog *)arg;
    while (w->running) {
        mrca_sleep_ms(500);
        if (!w->running) break;
        long age = (long)(mrca_now_ms() - w->last_pet_ms);
        if (age > w->deadline_ms) {
            LOGI("watchdog: session loop starved %ldms > %ldms deadline",
                 age, w->deadline_ms);
            w->tripped = 1;
            if (w->on_trip) w->on_trip();
            break;
        }
    }
    return NULL;
}

int watchdog_start(struct watchdog *w, long deadline_ms, void (*on_trip)(void)) {
    w->deadline_ms = deadline_ms;
    w->on_trip = on_trip;
    w->tripped = 0;
    w->running = 1;
    w->last_pet_ms = (long)mrca_now_ms();
    pthread_t t;
    if (pthread_create(&t, NULL, watchdog_thread, w) != 0) return -1;
    pthread_detach(t);
    return 0;
}

void watchdog_pet(struct watchdog *w) {
    w->last_pet_ms = (long)mrca_now_ms();
}

void watchdog_stop(struct watchdog *w) {
    w->running = 0;
}
