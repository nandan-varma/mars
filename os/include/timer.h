#ifndef TIMER_H
#define TIMER_H

#include "uefi.h"

void timer_init(UINTN hz);
void timer_poll(void);
UINT64 timer_ticks(void);
UINTN timer_hz(void);

#endif
