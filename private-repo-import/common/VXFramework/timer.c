// -------------------------------------------------------------
//  Cubzh Core
//  timer.c
//  Created by Xavier Legland on October 28, 2021.
// -------------------------------------------------------------

#include "timer.h"

// C
#include <stdlib.h>

#include "xptools.h"

typedef struct _Timer {
    TICK_DELTA_SEC_T time;
    TICK_DELTA_SEC_T elapsedTime;
    bool repeat;
    timerFunction callback;
    void *userData;
    bool toDelete;
    timer_state state;
} Timer;

Timer *timer_new(const TICK_DELTA_SEC_T time,
                 const bool repeat,
                 timerFunction callback,
                 void *userData) {
    Timer * const t = (Timer *)malloc(sizeof(Timer));
    if (t == NULL) { return NULL; }
    t->time = time;
    t->elapsedTime = 0.0;
    t->repeat = repeat;
    t->callback = callback;
    t->userData = userData;
    t->toDelete = false;
    t->state = running;
    return t;
}

void timer_free(Timer * const t) {
    free(t);
}

void timer_free_std(void * const t) {
    timer_free((Timer *)t);
}

void timer_increment(Timer * const t, TICK_DELTA_SEC_T dt) {
    if (t->state != running) {
        return;
    }
    t->elapsedTime += dt;
    if (t->elapsedTime >= t->time) {
        if (t->callback != NULL) {
            (t->callback)(t, t->userData);
        }
        if (t->repeat == false) {
            t->state = ended;
        }
        t->elapsedTime = 0.0;
    }
}

void timer_cancel(Timer * const t) {
    t->state = ended;
}

void timer_reset(Timer * const t) {
    t->elapsedTime = 0;
    if (t->state == ended) {
        t->state = running;
    }
}

void timer_pause(Timer * const t) {
    if (t->state == running) {
        t->state = paused;
    }
}

void timer_resume(Timer * const t) {
    if (t->state == paused) {
        t->state = running;
    }
}

bool timer_get_repeat(const Timer * const t) {
    return t->repeat;
}

TICK_DELTA_SEC_T timer_get_remaining_time(const Timer * const t) {
    if (t == NULL) {
        return 0.0;
    }
    return t->time - t->elapsedTime;
}

void timer_flagForDeletion(Timer * const t) {
    t->toDelete = true;
}

bool timer_isFlaggedForDeletion(const Timer * const t) {
    return t->toDelete;
}

timer_state timer_get_state(Timer * const t) {
    return t->state;
}
