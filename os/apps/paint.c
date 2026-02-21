#include "uefi.h"
#include "sdk/sdk_core.h"
#include "sdk/sdk_graphics.h"
#include "sdk/sdk_input.h"
#include "sdk/sdk_ui.h"

// Simple paint app - demonstrates graphics drawing

typedef struct {
    sdk_component_t *color_buttons[6];
    sdk_component_t *brush_slider;
    sdk_component_t *clear_button;
    UINT32 current_color;
    INT32 brush_size;
    BOOLEAN drawing;
} paint_state_t;

static paint_state_t *g_paint_state = NULL;

BOOLEAN paint_init(UINT32 window_id) {
    g_paint_state = (paint_state_t *)heap_alloc(sizeof(paint_state_t));
    if (g_paint_state == NULL) {
        return FALSE;
    }
    
    g_paint_state->current_color = SDK_COLOR_BLACK;
    g_paint_state->brush_size = 3;
    g_paint_state->drawing = FALSE;
    
    return TRUE;
}

void paint_render(void) {
    if (g_paint_state == NULL) {
        return;
    }
    
    sdk_graphics_clear(SDK_COLOR_WHITE);
    sdk_graphics_text(10, 10, L"Paint - Draw with mouse", SDK_COLOR_TEXT_PRIMARY, SDK_COLOR_WHITE);
    sdk_graphics_present();
}

void paint_handle_input(const input_event_t *event) {
    if (g_paint_state == NULL || event == NULL) {
        return;
    }
    
    if (sdk_input_is_left_pressed(event)) {
        INT32 x, y;
        if (sdk_input_get_mouse_pos(event, &x, &y)) {
            sdk_graphics_circle(x, y, g_paint_state->brush_size, g_paint_state->current_color);
        }
    }
}

void paint_cleanup(void) {
    if (g_paint_state != NULL) {
        heap_free((EFI_PHYSICAL_ADDRESS)(UINTN)g_paint_state);
        g_paint_state = NULL;
    }
}
