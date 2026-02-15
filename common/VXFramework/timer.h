// -------------------------------------------------------------
//  Cubzh Core
//  timer.h
//  Created by Xavier Legland on October 28, 2021.
// -------------------------------------------------------------

#pragma once

// C
#include <stdbool.h>

#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _Timer Timer;

typedef void (*timerFunction)(Timer * const, void *userData);

typedef enum {
    running = 1,
    paused,
    ended,
} timer_state;

Timer *timer_new(const TICK_DELTA_SEC_T time,
                 const bool repeat,
                 timerFunction callback,
                 void *userData);

void timer_free(Timer * const t);
void timer_free_std(void * const t);

/// increments Timer's time and calls its callback
/// once it has reached its set time
void timer_increment(Timer * const t, TICK_DELTA_SEC_T dt);

void timer_cancel(Timer * const t);
void timer_reset(Timer * const t);
void timer_pause(Timer * const t);
void timer_resume(Timer * const t);

bool timer_get_repeat(const Timer * const t);

void timer_flagForDeletion(Timer * const t);
bool timer_isFlaggedForDeletion(const Timer * const t);
timer_state timer_get_state(Timer *const t);

TICK_DELTA_SEC_T timer_get_remaining_time(const Timer * const t);

#ifdef __cplusplus
}
#endif
