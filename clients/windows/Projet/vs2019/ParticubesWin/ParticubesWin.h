#pragma once

#define CLOCK_MONOTONIC 0 
#define CLOCK_MONOTONIC_RAW 1

// ------------------------------
// C symbols
// ------------------------------

#ifdef __cplusplus
extern "C" {
#endif

#include <time.h>

int clock_gettime(int type, struct timespec *tp);

#ifdef __cplusplus
} // extern "C"
#endif
