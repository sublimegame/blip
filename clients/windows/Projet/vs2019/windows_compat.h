#pragma once

#include <time.h>

#define CLOCK_MONOTONIC 0 
#define CLOCK_MONOTONIC_RAW 1

int clock_gettime(int type, struct timespec* tp);
