#include "kernel.h"
#include "framebuffer.h"
#include "input.h"
#include "gui.h"
#include "timer.h"
#include "memory.h"

void kernel_main(const boot_info_t *boot_info) {
    framebuffer_init(boot_info);
    clearScreen(0x00101820);
    drawString(24, 24, L"MarsOS: UEFI kernel online", 0x00FFFFFF, 0x00101820);

    input_init(boot_info, boot_info->width, boot_info->height);
    gui_init(boot_info);
    timer_init(boot_info);

    memory_init(boot_info);

    EFI_PHYSICAL_ADDRESS warmup_page = memory_alloc_pages(1);
    (void)warmup_page;

    gui_update();
    gui_render();
    framebuffer_present();

    INT32 last_mouse_x = input_mouse_x();
    INT32 last_mouse_y = input_mouse_y();
    BOOLEAN last_left = input_left_down();
    UINTN idle_frames = 0;
    for (;;) {
        input_poll();

        BOOLEAN had_event = FALSE;
        input_event_t event;
        while (input_pop_event(&event)) {
            had_event = TRUE;
            gui_handle_event(&event);
        }

        INT32 mouse_x = input_mouse_x();
        INT32 mouse_y = input_mouse_y();
        BOOLEAN left_now = input_left_down();

        BOOLEAN pointer_changed = (mouse_x != last_mouse_x) || (mouse_y != last_mouse_y) || (left_now != last_left);
        if (pointer_changed) {
            last_mouse_x = mouse_x;
            last_mouse_y = mouse_y;
            last_left = left_now;
        }

        if (had_event || pointer_changed) {
            idle_frames = 0;
        } else {
            ++idle_frames;
        }

        BOOLEAN should_render = had_event || pointer_changed || idle_frames >= 30;
        if (should_render) {
            gui_update();
            gui_render();
            framebuffer_present();
            idle_frames = 0;
        }

        timer_frame_wait(60);
    }
}
