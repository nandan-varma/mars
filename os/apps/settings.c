#include "uefi.h"
#include "heap.h"
#include "sdk/sdk_core.h"
#include "sdk/sdk_graphics.h"
#include "sdk/sdk_input.h"
#include "sdk/sdk_ui.h"

// Settings app - demonstrates sliders, checkboxes, and radio buttons

typedef struct {
    sdk_component_t *brightness_slider;
    sdk_component_t *volume_slider;
    sdk_component_t *fullscreen_checkbox;
    sdk_component_t *vsync_checkbox;
    INT32 brightness;
    INT32 volume;
    BOOLEAN fullscreen;
    BOOLEAN vsync;
} settings_state_t;

static settings_state_t *g_settings_state = NULL;

BOOLEAN settings_init(UINT32 window_id) {
    (void)window_id;
    g_settings_state = (settings_state_t *)heap_alloc(sizeof(settings_state_t));
    if (g_settings_state == NULL) {
        return FALSE;
    }
    
    g_settings_state->brightness = 80;
    g_settings_state->volume = 60;
    g_settings_state->fullscreen = FALSE;
    g_settings_state->vsync = TRUE;
    
    // Create sliders
    g_settings_state->brightness_slider = sdk_component_create(SDK_COMPONENT_SLIDER);
    g_settings_state->volume_slider = sdk_component_create(SDK_COMPONENT_SLIDER);
    
    if (g_settings_state->brightness_slider == NULL || g_settings_state->volume_slider == NULL) {
        return FALSE;
    }
    
    sdk_component_set_position(g_settings_state->brightness_slider, 20, 60);
    sdk_component_set_size(g_settings_state->brightness_slider, 200, 30);
    sdk_slider_set_range(g_settings_state->brightness_slider, 0, 100);
    sdk_slider_set_value(g_settings_state->brightness_slider, g_settings_state->brightness);
    
    sdk_component_set_position(g_settings_state->volume_slider, 20, 110);
    sdk_component_set_size(g_settings_state->volume_slider, 200, 30);
    sdk_slider_set_range(g_settings_state->volume_slider, 0, 100);
    sdk_slider_set_value(g_settings_state->volume_slider, g_settings_state->volume);
    
    return TRUE;
}

void settings_render(void) {
    if (g_settings_state == NULL) {
        return;
    }
    
    sdk_graphics_clear(SDK_COLOR_BG_LIGHT);
    sdk_graphics_text(10, 10, L"Settings", SDK_COLOR_TEXT_PRIMARY, SDK_COLOR_BG_LIGHT);
    sdk_graphics_text(10, 25, L"Use sliders to adjust values", SDK_COLOR_TEXT_SECONDARY, SDK_COLOR_BG_LIGHT);
    
    // Brightness slider
    sdk_graphics_text(20, 50, L"Brightness:", SDK_COLOR_TEXT_PRIMARY, SDK_COLOR_BG_LIGHT);
    sdk_component_render(g_settings_state->brightness_slider);
    
    // Volume slider
    sdk_graphics_text(20, 110, L"Volume:", SDK_COLOR_TEXT_PRIMARY, SDK_COLOR_BG_LIGHT);
    sdk_component_render(g_settings_state->volume_slider);
    
    sdk_graphics_present();
}

void settings_handle_input(const input_event_t *event) {
    if (g_settings_state == NULL || event == NULL) {
        return;
    }
    
    sdk_component_handle_input(g_settings_state->brightness_slider, event);
    sdk_component_handle_input(g_settings_state->volume_slider, event);
}

void settings_cleanup(void) {
    if (g_settings_state == NULL) {
        return;
    }
    
    if (g_settings_state->brightness_slider != NULL) {
        sdk_component_destroy(g_settings_state->brightness_slider);
    }
    if (g_settings_state->volume_slider != NULL) {
        sdk_component_destroy(g_settings_state->volume_slider);
    }
    
        heap_free(g_settings_state);
    g_settings_state = NULL;
}
