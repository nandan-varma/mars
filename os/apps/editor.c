#include "uefi.h"
#include "heap.h"
#include "sdk/sdk_core.h"
#include "sdk/sdk_graphics.h"
#include "sdk/sdk_input.h"
#include "sdk/sdk_ui.h"

// Text Editor app - demonstrates text fields and scrolling

typedef struct {
    sdk_component_t *text_field;
    sdk_component_t *scroll_slider;
    sdk_component_t *save_button;
    sdk_component_t *open_button;
    INT32 scroll_offset;
} editor_state_t;

static editor_state_t *g_editor_state = NULL;

BOOLEAN editor_init(UINT32 window_id) {
    (void)window_id;
    g_editor_state = (editor_state_t *)heap_alloc(sizeof(editor_state_t));
    if (g_editor_state == NULL) {
        return FALSE;
    }
    
    g_editor_state->scroll_offset = 0;
    
    // Create text field
    g_editor_state->text_field = sdk_component_create(SDK_COMPONENT_TEXT_FIELD);
    if (g_editor_state->text_field == NULL) {
        return FALSE;
    }
    
    sdk_component_set_position(g_editor_state->text_field, 10, 40);
    sdk_component_set_size(g_editor_state->text_field, 500, 300);
    sdk_textfield_set_placeholder(g_editor_state->text_field, L"Type your text here...");
    
    // Create scroll slider
    g_editor_state->scroll_slider = sdk_component_create(SDK_COMPONENT_SLIDER);
    if (g_editor_state->scroll_slider == NULL) {
        return FALSE;
    }
    
    sdk_component_set_position(g_editor_state->scroll_slider, 520, 40);
    sdk_component_set_size(g_editor_state->scroll_slider, 20, 300);
    sdk_slider_set_range(g_editor_state->scroll_slider, 0, 100);
    sdk_slider_set_orientation(g_editor_state->scroll_slider, FALSE);  // Vertical
    
    return TRUE;
}

void editor_render(void) {
    if (g_editor_state == NULL) {
        return;
    }
    
    sdk_graphics_clear(SDK_COLOR_BG_LIGHT);
    sdk_graphics_text(10, 10, L"Text Editor", SDK_COLOR_TEXT_PRIMARY, SDK_COLOR_BG_LIGHT);
    sdk_graphics_text(150, 10, L"(Type to edit, scroll with slider)", SDK_COLOR_TEXT_SECONDARY, SDK_COLOR_BG_LIGHT);
    
    // Draw line numbers on the left
    sdk_graphics_line(35, 35, 35, 345, SDK_COLOR_BORDER);
    sdk_graphics_text(12, 45, L"1", SDK_COLOR_TEXT_SECONDARY, SDK_COLOR_BG_LIGHT);
    sdk_graphics_text(12, 85, L"2", SDK_COLOR_TEXT_SECONDARY, SDK_COLOR_BG_LIGHT);
    sdk_graphics_text(12, 125, L"3", SDK_COLOR_TEXT_SECONDARY, SDK_COLOR_BG_LIGHT);
    sdk_graphics_text(12, 165, L"4", SDK_COLOR_TEXT_SECONDARY, SDK_COLOR_BG_LIGHT);
    sdk_graphics_text(12, 205, L"5", SDK_COLOR_TEXT_SECONDARY, SDK_COLOR_BG_LIGHT);
    
    sdk_component_render(g_editor_state->text_field);
    sdk_component_render(g_editor_state->scroll_slider);
}

void editor_handle_input(const input_event_t *event) {
    if (g_editor_state == NULL || event == NULL) {
        return;
    }
    
    sdk_component_handle_input(g_editor_state->text_field, event);
    sdk_component_handle_input(g_editor_state->scroll_slider, event);
}

void editor_cleanup(void) {
    if (g_editor_state == NULL) {
        return;
    }
    
    if (g_editor_state->text_field != NULL) {
        sdk_component_destroy(g_editor_state->text_field);
    }
    if (g_editor_state->scroll_slider != NULL) {
        sdk_component_destroy(g_editor_state->scroll_slider);
    }
    
    heap_free(g_editor_state);
    g_editor_state = NULL;
}
