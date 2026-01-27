
#include "windows_compat.h"

//#include "targetver.h"
//#define WIN32_LEAN_AND_MEAN // Exclure les en-têtes Windows rarement utilisés
//// Fichiers d'en-tête C RunTime
//#include <stdlib.h>
//// Fichiers d'en-tête Windows
//#include <malloc.h>
//#include <memory.h>
//#include <ostream>
//#include <tchar.h>
//#include <iostream>

#include <windows.h>
//#include <winnt.h>
//#include <minwindef.h>

// For monotonic, I just take the perf counter since a wall clock baseline is
// meaningless.

#define MS_PER_SEC 1000ULL // MS = milliseconds
#define US_PER_MS 1000ULL  // US = microseconds
#define HNS_PER_US 10ULL   // HNS = hundred-nanoseconds (e.g., 1 hns = 100 ns)
#define NS_PER_US 1000ULL

#define HNS_PER_SEC (MS_PER_SEC * US_PER_MS * HNS_PER_US)
#define NS_PER_HNS (100ULL) // NS = nanoseconds
#define NS_PER_SEC (MS_PER_SEC * US_PER_MS * NS_PER_US)

int clock_gettime_monotonic(struct timespec* tv) {
    static LARGE_INTEGER ticksPerSec;
    LARGE_INTEGER ticks;
    double seconds;

    if (!ticksPerSec.QuadPart) {
        QueryPerformanceFrequency(&ticksPerSec);
        if (!ticksPerSec.QuadPart) {
            errno = ENOTSUP;
            return -1;
        }
    }

    QueryPerformanceCounter(&ticks);

    seconds = (double)ticks.QuadPart / (double)ticksPerSec.QuadPart;
    tv->tv_sec = (time_t)seconds;
    tv->tv_nsec = (long)((ULONGLONG)(seconds * NS_PER_SEC) % NS_PER_SEC);

    return 0;
}

// and wall clock, based on GMT unlike the tempting and similar _ftime()
// function.

int clock_gettime_realtime(struct timespec* tv) {
    FILETIME ft;
    ULARGE_INTEGER hnsTime;

    GetSystemTimeAsFileTime(&ft);

    hnsTime.LowPart = ft.dwLowDateTime;
    hnsTime.HighPart = ft.dwHighDateTime;

    // To get POSIX Epoch as baseline, subtract the number of hns intervals from
    // Jan 1, 1601 to Jan 1, 1970.
    hnsTime.QuadPart -= (11644473600ULL * HNS_PER_SEC);

    // modulus by hns intervals per second first, then convert to ns, as not to
    // lose resolution
    tv->tv_nsec = (long)((hnsTime.QuadPart % HNS_PER_SEC) * NS_PER_HNS);
    tv->tv_sec = (long)(hnsTime.QuadPart / HNS_PER_SEC);

    return 0;
}

int clock_gettime(int type, struct timespec* tp) {
    if (type == CLOCK_MONOTONIC) {
        return clock_gettime_monotonic(tp);
    }
    else if (type == CLOCK_MONOTONIC_RAW) {
        return clock_gettime_realtime(tp);
    }
    return -1;
}