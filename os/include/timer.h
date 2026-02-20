#ifndef TIMER_H
#define TIMER_H

#include "uefi.h"
#include "boot_info.h"

void timer_init(const boot_info_t *boot_info);
void timer_frame_wait(UINTN target_fps);

#endif
