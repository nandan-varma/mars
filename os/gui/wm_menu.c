#include "internal/wm_internal.h"

#include "framebuffer.h"
#include "input.h"
#include "wm_utils.h"

BOOLEAN wm_handle_start_menu_click(INT32 mouse_x, INT32 mouse_y) {
    wm_state_t *state = wm_state();

    INT32 taskbar_y = (INT32)state->desktop_h - TASKBAR_H;
    INT32 start_x = 6;
    INT32 start_y = taskbar_y + 4;

    if (wm_point_in_rect(mouse_x, mouse_y, start_x, start_y, START_BUTTON_W, START_BUTTON_H)) {
        state->start_menu_open = !state->start_menu_open;
        state->dirty = TRUE;
        return TRUE;
    }

    if (!state->start_menu_open) {
        return FALSE;
    }

    INT32 menu_x = 6;
    INT32 menu_h = START_MENU_ITEM_COUNT * START_MENU_ITEM_H + 8;
    INT32 menu_y = taskbar_y - menu_h;

    if (!wm_point_in_rect(mouse_x, mouse_y, menu_x, menu_y, START_MENU_W, menu_h)) {
        state->start_menu_open = FALSE;
        state->dirty = TRUE;
        return FALSE;
    }

    INT32 local_y = mouse_y - (menu_y + 28);
    if (local_y < 0) {
        return TRUE;
    }

    UINTN index = (UINTN)(local_y / START_MENU_ITEM_H);
    if (index < START_MENU_ITEM_COUNT) {
        UINTN item_count = 0;
        const start_item_t *items = wm_start_items(&item_count);
        if (items != NULL && index < item_count) {
            wm_publish_launch_request(items[index].id);
        }
        state->start_menu_open = FALSE;
        state->dirty = TRUE;
    }

    return TRUE;
}

void wm_render_start_menu(void) {
    wm_state_t *state = wm_state();
    if (!state->start_menu_open) {
        return;
    }

    INT32 taskbar_y = (INT32)state->desktop_h - TASKBAR_H;
    INT32 menu_x = 6;
    INT32 menu_h = START_MENU_ITEM_COUNT * START_MENU_ITEM_H + 8;
    INT32 menu_y = taskbar_y - menu_h;
    INT32 mouse_x = input_mouse_x();
    INT32 mouse_y = input_mouse_y();

    drawRect(menu_x + 2, menu_y + 2, START_MENU_W, menu_h, 0x00101928);
    drawRect(menu_x, menu_y, START_MENU_W, menu_h, 0x00354456);
    drawRect(menu_x + 1, menu_y + 1, START_MENU_W - 2, menu_h - 2, 0x00EAF0F8);
    drawRect(menu_x + 1, menu_y + 1, START_MENU_W - 2, 24, 0x00315FAA);
    drawString(menu_x + 10, menu_y + 6, L"Applications", 0x00FFFFFF, 0x00315FAA);

    UINTN item_count = 0;
    const start_item_t *items = wm_start_items(&item_count);
    if (items == NULL) {
        return;
    }

    for (UINTN i = 0; i < item_count; ++i) {
        INT32 item_y = menu_y + 28 + (INT32)i * START_MENU_ITEM_H;
        BOOLEAN hovered = wm_point_in_rect(mouse_x, mouse_y, menu_x + 4, item_y, START_MENU_W - 8, START_MENU_ITEM_H - 2);
        UINT32 item_color = hovered ? 0x00CFE0F8 : 0x00DDE7F2;
        UINT32 text_color = hovered ? 0x000B2B55 : 0x00101820;
        drawRect(menu_x + 4, item_y, START_MENU_W - 8, START_MENU_ITEM_H - 2, item_color);
        drawString(menu_x + 10, item_y + 7, items[i].label, text_color, item_color);
    }
}
