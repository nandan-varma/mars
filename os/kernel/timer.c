#include "timer.h"

static UINT64 g_ticks;
static UINTN g_hz;

void timer_init(UINTN hz) {
    g_ticks = 0;
    g_hz = hz == 0 ? 1000 : hz;
}

void timer_poll(void) {
    ++g_ticks;
}

UINT64 timer_ticks(void) {
    return g_ticks;
}

UINTN timer_hz(void) {
    return g_hz;
}