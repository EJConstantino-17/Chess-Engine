#ifndef CCE_TIMING_H
#define CCE_TIMING_H

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
static inline double cce_now_ms(void) {
    LARGE_INTEGER count, frequency;
    if (QueryPerformanceFrequency(&frequency) && QueryPerformanceCounter(&count))
        return 1000.0 * (double)count.QuadPart / (double)frequency.QuadPart;
    return (double)GetTickCount64();
}
#else
#include <time.h>
static inline double cce_now_ms(void) {
    struct timespec value;
#ifdef CLOCK_MONOTONIC
    if (clock_gettime(CLOCK_MONOTONIC, &value) == 0)
        return 1000.0 * value.tv_sec + value.tv_nsec / 1000000.0;
#endif
    timespec_get(&value, TIME_UTC);
    return 1000.0 * value.tv_sec + value.tv_nsec / 1000000.0;
}
#endif
#endif
