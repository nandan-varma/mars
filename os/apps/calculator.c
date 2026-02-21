#include "uefi.h"
#include "app.h"
#include "process.h"
#include "scheduler.h"
#include "heap.h"
#include "sdk/sdk_core.h"
#include "sdk/sdk_graphics.h"
#include "sdk/sdk_input.h"
#include "sdk/sdk_ui.h"

// ============================================================================
// Calculator App State
// ============================================================================

typedef struct {
    sdk_component_t *display_label;
    sdk_component_t *buttons[16];  // 0-9, +, -, *, /
    sdk_component_t *equals_button;
    sdk_component_t *clear_button;
    
    CHAR16 display_text[64];
    INT64 current_value;
    INT64 previous_value;
    CHAR16 operation;
    BOOLEAN new_number;
    INT32 window_id;
} calculator_state_t;

static calculator_state_t *g_calc_state = NULL;

// ============================================================================
// Callback Functions
// ============================================================================

static BOOLEAN on_number_button_click(sdk_component_t *button) {
    if (g_calc_state == NULL || button == NULL) {
        return FALSE;
    }
    
    // Find which number button was clicked
    CHAR16 number_char = L'0';
    for (INT32 i = 0; i < 10; i++) {
        if (g_calc_state->buttons[i] == button) {
            number_char = L'0' + i;
            break;
        }
    }
    
    if (g_calc_state->new_number) {
        sdk_string_t display = sdk_string_create(g_calc_state->display_text, 64);
        sdk_string_clear(&display);
        sdk_string_append_char(&display, number_char);
        g_calc_state->new_number = FALSE;
    } else {
        sdk_string_t display = sdk_string_create(g_calc_state->display_text, 64);
        sdk_string_append_char(&display, number_char);
    }
    
    sdk_label_set_text(g_calc_state->display_label, g_calc_state->display_text);
    return TRUE;
}

static BOOLEAN on_operation_button_click(sdk_component_t *button) {
    if (g_calc_state == NULL || button == NULL) {
        return FALSE;
    }
    
    // Parse current display value
    sdk_string_t display = sdk_string_create(g_calc_state->display_text, 64);
    INT64 new_value = 0;
    for (UINTN i = 0; i < display.current_len; i++) {
        CHAR16 ch = display.buffer[i];
        if (ch >= L'0' && ch <= L'9') {
            new_value = new_value * 10 + (ch - L'0');
        }
    }
    
    if (g_calc_state->operation != L'\0') {
        // Perform pending operation
        switch (g_calc_state->operation) {
            case L'+':
                g_calc_state->current_value = g_calc_state->previous_value + new_value;
                break;
            case L'-':
                g_calc_state->current_value = g_calc_state->previous_value - new_value;
                break;
            case L'*':
                g_calc_state->current_value = g_calc_state->previous_value * new_value;
                break;
            case L'/':
                if (new_value != 0) {
                    g_calc_state->current_value = g_calc_state->previous_value / new_value;
                }
                break;
        }
    } else {
        g_calc_state->current_value = new_value;
    }
    
    // Find which operation button was clicked
    CHAR16 op = L'+';
    for (INT32 i = 10; i < 14; i++) {
        if (g_calc_state->buttons[i] == button) {
            switch (i) {
                case 10: op = L'+'; break;
                case 11: op = L'-'; break;
                case 12: op = L'*'; break;
                case 13: op = L'/'; break;
            }
            break;
        }
    }
    
    g_calc_state->operation = op;
    g_calc_state->previous_value = g_calc_state->current_value;
    g_calc_state->new_number = TRUE;
    
    // Clear display for next number
    sdk_string_clear(&display);
    sdk_label_set_text(g_calc_state->display_label, L"");
    
    return TRUE;
}

static BOOLEAN on_equals_click(sdk_component_t *button) {
    if (g_calc_state == NULL || button == NULL) {
        return FALSE;
    }
    
    // Parse current display value
    sdk_string_t display = sdk_string_create(g_calc_state->display_text, 64);
    INT64 new_value = 0;
    for (UINTN i = 0; i < display.current_len; i++) {
        CHAR16 ch = display.buffer[i];
        if (ch >= L'0' && ch <= L'9') {
            new_value = new_value * 10 + (ch - L'0');
        }
    }
    
    if (g_calc_state->operation != L'\0') {
        switch (g_calc_state->operation) {
            case L'+':
                g_calc_state->current_value = g_calc_state->previous_value + new_value;
                break;
            case L'-':
                g_calc_state->current_value = g_calc_state->previous_value - new_value;
                break;
            case L'*':
                g_calc_state->current_value = g_calc_state->previous_value * new_value;
                break;
            case L'/':
                if (new_value != 0) {
                    g_calc_state->current_value = g_calc_state->previous_value / new_value;
                }
                break;
        }
        g_calc_state->operation = L'\0';
    }
    
    // Update display with result
    sdk_string_clear(&display);
    sdk_string_append_int(&display, g_calc_state->current_value);
    sdk_label_set_text(g_calc_state->display_label, g_calc_state->display_text);
    
    g_calc_state->new_number = TRUE;
    return TRUE;
}

static BOOLEAN on_clear_click(sdk_component_t *button) {
    if (g_calc_state == NULL || button == NULL) {
        return FALSE;
    }
    
    g_calc_state->current_value = 0;
    g_calc_state->previous_value = 0;
    g_calc_state->operation = L'\0';
    g_calc_state->new_number = TRUE;
    
    sdk_string_t display = sdk_string_create(g_calc_state->display_text, 64);
    sdk_string_clear(&display);
    sdk_label_set_text(g_calc_state->display_label, L"0");
    
    return TRUE;
}

// ============================================================================
// App Initialization
// ============================================================================

BOOLEAN calculator_init(UINT32 window_id) {
    (void)window_id;
    g_calc_state = (calculator_state_t *)heap_alloc(sizeof(calculator_state_t));
    if (g_calc_state == NULL) {
        return FALSE;
    }
    
    g_calc_state->window_id = window_id;
    g_calc_state->display_text[0] = L'0';
    g_calc_state->display_text[1] = L'\0';
    g_calc_state->current_value = 0;
    g_calc_state->previous_value = 0;
    g_calc_state->operation = L'\0';
    g_calc_state->new_number = TRUE;
    
    // Create display label
    g_calc_state->display_label = sdk_component_create(SDK_COMPONENT_LABEL);
    if (g_calc_state->display_label == NULL) {
        return FALSE;
    }
    sdk_component_set_position(g_calc_state->display_label, 10, 10);
    sdk_component_set_size(g_calc_state->display_label, 280, 40);
    sdk_label_set_text(g_calc_state->display_label, g_calc_state->display_text);
    sdk_label_set_color(g_calc_state->display_label, SDK_COLOR_WHITE, SDK_COLOR_BLUE);
    
    // Create number buttons (0-9)
    INT32 x = 10, y = 60;
    for (INT32 i = 0; i < 10; i++) {
        g_calc_state->buttons[i] = sdk_component_create(SDK_COMPONENT_BUTTON);
        if (g_calc_state->buttons[i] == NULL) {
            return FALSE;
        }
        
        INT32 col = (i == 0) ? 1 : (i - 1) % 3;
        INT32 row = (i == 0) ? 3 : (i - 1) / 3;
        
        sdk_component_set_position(g_calc_state->buttons[i], x + col * 100, y + row * 50);
        sdk_component_set_size(g_calc_state->buttons[i], 90, 40);
        
        CHAR16 label[2] = {L'0' + i, L'\0'};
        sdk_button_set_label(g_calc_state->buttons[i], label);
        sdk_component_on_click(g_calc_state->buttons[i], on_number_button_click);
    }
    
    // Create operation buttons (+, -, *, /)
    const CHAR16 *ops[] = {L"+", L"-", L"*", L"/"};
    for (INT32 i = 0; i < 4; i++) {
        g_calc_state->buttons[10 + i] = sdk_component_create(SDK_COMPONENT_BUTTON);
        if (g_calc_state->buttons[10 + i] == NULL) {
            return FALSE;
        }
        
        sdk_component_set_position(g_calc_state->buttons[10 + i], 310, y + i * 50);
        sdk_component_set_size(g_calc_state->buttons[10 + i], 50, 40);
        sdk_button_set_label(g_calc_state->buttons[10 + i], ops[i]);
        sdk_component_on_click(g_calc_state->buttons[10 + i], on_operation_button_click);
    }
    
    // Create equals button
    g_calc_state->equals_button = sdk_component_create(SDK_COMPONENT_BUTTON);
    if (g_calc_state->equals_button == NULL) {
        return FALSE;
    }
    sdk_component_set_position(g_calc_state->equals_button, 310, 200);
    sdk_component_set_size(g_calc_state->equals_button, 50, 40);
    sdk_button_set_label(g_calc_state->equals_button, L"=");
    sdk_button_set_color(g_calc_state->equals_button, SDK_COLOR_GREEN, SDK_COLOR_ACCENT_LIGHT, SDK_COLOR_ACCENT);
    sdk_component_on_click(g_calc_state->equals_button, on_equals_click);
    
    // Create clear button
    g_calc_state->clear_button = sdk_component_create(SDK_COMPONENT_BUTTON);
    if (g_calc_state->clear_button == NULL) {
        return FALSE;
    }
    sdk_component_set_position(g_calc_state->clear_button, 310, 250);
    sdk_component_set_size(g_calc_state->clear_button, 50, 40);
    sdk_button_set_label(g_calc_state->clear_button, L"C");
    sdk_button_set_color(g_calc_state->clear_button, SDK_COLOR_RED, SDK_COLOR_ACCENT, SDK_COLOR_ACCENT_DARK);
    sdk_component_on_click(g_calc_state->clear_button, on_clear_click);
    
    return TRUE;
}

// ============================================================================
// App Rendering
// ============================================================================

void calculator_render(void) {
    if (g_calc_state == NULL) {
        return;
    }
    
    // Clear screen
    sdk_graphics_clear(SDK_COLOR_BG_LIGHT);
    
    // Draw title
    sdk_graphics_text(10, 0, L"Calculator", SDK_COLOR_TEXT_PRIMARY, SDK_COLOR_BG_LIGHT);
    
    // Render all components
    sdk_component_render(g_calc_state->display_label);
    
    for (INT32 i = 0; i < 14; i++) {
        sdk_component_render(g_calc_state->buttons[i]);
    }
    
    sdk_component_render(g_calc_state->equals_button);
    sdk_component_render(g_calc_state->clear_button);
    
    // Present changes
    sdk_graphics_present();
}

// ============================================================================
// App Input Handling
// ============================================================================

void calculator_handle_input(const input_event_t *event) {
    if (g_calc_state == NULL || event == NULL) {
        return;
    }
    
    // Route input to components
    for (INT32 i = 0; i < 14; i++) {
        if (sdk_component_handle_input(g_calc_state->buttons[i], event)) {
            return;
        }
    }
    
    if (sdk_component_handle_input(g_calc_state->equals_button, event)) {
        return;
    }
    
    if (sdk_component_handle_input(g_calc_state->clear_button, event)) {
        return;
    }
}

// ============================================================================
// App Lifecycle (Stub)
// ============================================================================

void calculator_cleanup(void) {
    if (g_calc_state == NULL) {
        return;
    }
    
    // Destroy all components
    if (g_calc_state->display_label != NULL) {
        sdk_component_destroy(g_calc_state->display_label);
    }
    
    for (INT32 i = 0; i < 14; i++) {
        if (g_calc_state->buttons[i] != NULL) {
            sdk_component_destroy(g_calc_state->buttons[i]);
        }
    }
    
    if (g_calc_state->equals_button != NULL) {
        sdk_component_destroy(g_calc_state->equals_button);
    }
    
    if (g_calc_state->clear_button != NULL) {
        sdk_component_destroy(g_calc_state->clear_button);
    }
    
    heap_free(g_calc_state);
    g_calc_state = NULL;
}

// ============================================================================
// App Main Entry Point (Integration stub)
// ============================================================================

// In a real integration, this would be called by the app framework
// For now, this demonstrates the SDK usage pattern
