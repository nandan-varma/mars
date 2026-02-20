#include "timer.h"

static EFI_BOOT_SERVICES *g_bs;

void timer_init(const boot_info_t *boot_info) {
    g_bs = boot_info->boot_services;
}

void timer_frame_wait(UINTN target_fps) {
    if (g_bs == NULL || target_fps == 0) {
        return;
    }

    UINTN microseconds = 1000000 / target_fps;
    g_bs->Stall(microseconds);
}
