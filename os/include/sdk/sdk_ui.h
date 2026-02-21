#ifndef SDK_UI_H
#define SDK_UI_H

#include "uefi.h"
#include "input.h"

// ============================================================================
// Component Type Enumeration
// ============================================================================

typedef enum {
    SDK_COMPONENT_BUTTON = 0,
    SDK_COMPONENT_TEXT_FIELD,
    SDK_COMPONENT_SLIDER,
    SDK_COMPONENT_LIST,
    SDK_COMPONENT_CHECKBOX,
    SDK_COMPONENT_RADIO,
    SDK_COMPONENT_LABEL,
    SDK_COMPONENT_PANEL,
    SDK_COMPONENT_DIALOG,
} sdk_component_type_t;

// ============================================================================
// Component Structure (Opaque)
// ============================================================================

typedef struct sdk_component sdk_component_t;

// ============================================================================
// Sensible Defaults
// ============================================================================

#define SDK_BUTTON_DEFAULT_WIDTH     80
#define SDK_BUTTON_DEFAULT_HEIGHT    32
#define SDK_BUTTON_COLOR_NORMAL      0xFF0078D4
#define SDK_BUTTON_COLOR_HOVER       0xFF005A9E
#define SDK_BUTTON_COLOR_PRESSED     0xFF003D82
#define SDK_BUTTON_COLOR_TEXT        0xFFFFFFFF

#define SDK_TEXTFIELD_DEFAULT_HEIGHT 32
#define SDK_TEXTFIELD_COLOR_BG       0xFFFFFFFF
#define SDK_TEXTFIELD_COLOR_BORDER   0xFF000000
#define SDK_TEXTFIELD_COLOR_TEXT     0xFF000000
#define SDK_TEXTFIELD_MAX_CHARS      256

#define SDK_SLIDER_DEFAULT_HEIGHT    24
#define SDK_SLIDER_TRACK_COLOR       0xFFE0E0E0
#define SDK_SLIDER_THUMB_COLOR       0xFF0078D4
#define SDK_SLIDER_THUMB_RADIUS      8

#define SDK_LIST_ITEM_HEIGHT         24
#define SDK_LIST_HIGHLIGHT_COLOR     0xFF0078D4

#define SDK_SPACING_COMPACT          4
#define SDK_SPACING_NORMAL           8
#define SDK_SPACING_LARGE            16

// ============================================================================
// Component Lifecycle
// ============================================================================

sdk_component_t* sdk_component_create(sdk_component_type_t type);
void sdk_component_destroy(sdk_component_t *comp);

// ============================================================================
// Component Positioning & Sizing
// ============================================================================

void sdk_component_set_position(sdk_component_t *comp, INT32 x, INT32 y);
void sdk_component_set_size(sdk_component_t *comp, INT32 w, INT32 h);
void sdk_component_get_bounds(sdk_component_t *comp, INT32 *out_x, INT32 *out_y, INT32 *out_w, INT32 *out_h);

// ============================================================================
// Component State Management
// ============================================================================

void sdk_component_set_enabled(sdk_component_t *comp, BOOLEAN enabled);
BOOLEAN sdk_component_is_enabled(sdk_component_t *comp);

void sdk_component_set_visible(sdk_component_t *comp, BOOLEAN visible);
BOOLEAN sdk_component_is_visible(sdk_component_t *comp);

void sdk_component_set_focused(sdk_component_t *comp, BOOLEAN focused);
BOOLEAN sdk_component_is_focused(sdk_component_t *comp);

// ============================================================================
// Component Rendering & Input
// ============================================================================

void sdk_component_render(sdk_component_t *comp);
BOOLEAN sdk_component_handle_input(sdk_component_t *comp, const input_event_t *event);

// ============================================================================
// Callbacks
// ============================================================================

typedef BOOLEAN (*sdk_on_click_t)(sdk_component_t *comp);
typedef BOOLEAN (*sdk_on_change_t)(sdk_component_t *comp);
typedef BOOLEAN (*sdk_on_submit_t)(sdk_component_t *comp);

void sdk_component_on_click(sdk_component_t *comp, sdk_on_click_t callback);
void sdk_component_on_change(sdk_component_t *comp, sdk_on_change_t callback);
void sdk_component_on_submit(sdk_component_t *comp, sdk_on_submit_t callback);

// ============================================================================
// BUTTON Component
// ============================================================================

void sdk_button_set_label(sdk_component_t *comp, const CHAR16 *label);
void sdk_button_set_color(sdk_component_t *comp, UINT32 normal_color, UINT32 hover_color, UINT32 pressed_color);
BOOLEAN sdk_button_is_pressed(sdk_component_t *comp);

// ============================================================================
// TEXT FIELD Component
// ============================================================================

void sdk_textfield_set_text(sdk_component_t *comp, const CHAR16 *text);
void sdk_textfield_get_text(sdk_component_t *comp, CHAR16 *out_buffer, UINTN max_len);
void sdk_textfield_set_placeholder(sdk_component_t *comp, const CHAR16 *placeholder);
void sdk_textfield_set_max_length(sdk_component_t *comp, UINTN max_len);
void sdk_textfield_clear(sdk_component_t *comp);

// ============================================================================
// SLIDER Component
// ============================================================================

void sdk_slider_set_range(sdk_component_t *comp, INT32 min, INT32 max);
void sdk_slider_set_value(sdk_component_t *comp, INT32 value);
INT32 sdk_slider_get_value(sdk_component_t *comp);
void sdk_slider_set_orientation(sdk_component_t *comp, BOOLEAN horizontal);

// ============================================================================
// LIST Component
// ============================================================================

typedef struct {
    CHAR16 label[64];
    BOOLEAN selectable;
    BOOLEAN is_separator;
} sdk_list_item_t;

void sdk_list_add_item(sdk_component_t *comp, const sdk_list_item_t *item);
void sdk_list_clear_items(sdk_component_t *comp);
INT32 sdk_list_get_selected_index(sdk_component_t *comp);
void sdk_list_set_selected_index(sdk_component_t *comp, INT32 index);
BOOLEAN sdk_list_get_selected_item(sdk_component_t *comp, sdk_list_item_t *out_item);

// ============================================================================
// CHECKBOX Component
// ============================================================================

void sdk_checkbox_set_label(sdk_component_t *comp, const CHAR16 *label);
void sdk_checkbox_set_checked(sdk_component_t *comp, BOOLEAN checked);
BOOLEAN sdk_checkbox_is_checked(sdk_component_t *comp);

// ============================================================================
// RADIO Button Component
// ============================================================================

typedef struct {
    UINT32 id;
    CHAR16 label[64];
} sdk_radio_option_t;

void sdk_radio_add_option(sdk_component_t *comp, const sdk_radio_option_t *option);
void sdk_radio_set_selected(sdk_component_t *comp, UINT32 option_id);
UINT32 sdk_radio_get_selected(sdk_component_t *comp);

// ============================================================================
// LABEL Component
// ============================================================================

void sdk_label_set_text(sdk_component_t *comp, const CHAR16 *text);
void sdk_label_set_color(sdk_component_t *comp, UINT32 fg, UINT32 bg);
void sdk_label_set_alignment(sdk_component_t *comp, INT32 align);

// ============================================================================
// PANEL Component
// ============================================================================

void sdk_panel_set_color(sdk_component_t *comp, UINT32 color);
void sdk_panel_set_border(sdk_component_t *comp, INT32 thickness, UINT32 color);

// ============================================================================
// DIALOG Component
// ============================================================================

void sdk_dialog_set_title(sdk_component_t *comp, const CHAR16 *title);
void sdk_dialog_set_message(sdk_component_t *comp, const CHAR16 *message);
void sdk_dialog_add_button(sdk_component_t *comp, const CHAR16 *label, INT32 button_id);
INT32 sdk_dialog_get_selected_button(sdk_component_t *comp);
BOOLEAN sdk_dialog_is_shown(sdk_component_t *comp);
void sdk_dialog_show(sdk_component_t *comp);
void sdk_dialog_hide(sdk_component_t *comp);

#endif // SDK_UI_H
