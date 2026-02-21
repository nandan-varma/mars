#include "sdk/sdk_ui.h"
#include "sdk/sdk_graphics.h"
#include "sdk/sdk_input.h"
#include "sdk/sdk_core.h"
#include "heap.h"
#include "os_string.h"

// ============================================================================
// Component Internal Structure
// ============================================================================

typedef struct sdk_component {
    sdk_component_type_t type;
    INT32 x, y, w, h;
    BOOLEAN enabled;
    BOOLEAN visible;
    BOOLEAN focused;
    
    // Callbacks
    sdk_on_click_t on_click;
    sdk_on_change_t on_change;
    sdk_on_submit_t on_submit;
    
    // Type-specific data
    union {
        // Button
        struct {
            CHAR16 label[64];
            UINT32 color_normal, color_hover, color_pressed;
            BOOLEAN hovered, pressed;
        } button;
        
        // Text Field
        struct {
            CHAR16 text[SDK_TEXTFIELD_MAX_CHARS];
            CHAR16 placeholder[64];
            UINTN text_len;
            UINTN cursor_pos;
            UINTN max_len;
            UINT32 color_bg, color_border, color_text;
        } textfield;
        
        // Slider
        struct {
            INT32 min, max, value;
            BOOLEAN horizontal;
            BOOLEAN dragging;
        } slider;
        
        // List
        struct {
            sdk_list_item_t items[32];
            UINTN item_count;
            INT32 selected_index;
            INT32 scroll_offset;
        } list;
        
        // Checkbox
        struct {
            CHAR16 label[64];
            BOOLEAN checked;
        } checkbox;
        
        // Radio
        struct {
            sdk_radio_option_t options[16];
            UINTN option_count;
            UINT32 selected_id;
        } radio;
        
        // Label
        struct {
            CHAR16 text[256];
            UINT32 fg_color, bg_color;
            INT32 alignment;  // 0=left, 1=center, 2=right
        } label;
        
        // Panel
        struct {
            UINT32 bg_color;
            INT32 border_thickness;
            UINT32 border_color;
        } panel;
        
        // Dialog
        struct {
            CHAR16 title[64];
            CHAR16 message[512];
            struct {
                CHAR16 label[64];
                INT32 button_id;
            } buttons[8];
            UINTN button_count;
            INT32 selected_button;
            BOOLEAN shown;
        } dialog;
    } data;
} sdk_component_t;

// ============================================================================
// Component Lifecycle
// ============================================================================

sdk_component_t* sdk_component_create(sdk_component_type_t type) {
    sdk_component_t *comp = (sdk_component_t *)heap_alloc(sizeof(sdk_component_t));
    if (comp == NULL) {
        return NULL;
    }
    
    // Initialize base component
    comp->type = type;
    comp->x = 0;
    comp->y = 0;
    comp->w = 100;
    comp->h = 30;
    comp->enabled = TRUE;
    comp->visible = TRUE;
    comp->focused = FALSE;
    comp->on_click = NULL;
    comp->on_change = NULL;
    comp->on_submit = NULL;
    
    // Initialize type-specific defaults
    switch (type) {
        case SDK_COMPONENT_BUTTON:
            comp->w = SDK_BUTTON_DEFAULT_WIDTH;
            comp->h = SDK_BUTTON_DEFAULT_HEIGHT;
            comp->data.button.label[0] = L'\0';
            comp->data.button.color_normal = SDK_BUTTON_COLOR_NORMAL;
            comp->data.button.color_hover = SDK_BUTTON_COLOR_HOVER;
            comp->data.button.color_pressed = SDK_BUTTON_COLOR_PRESSED;
            comp->data.button.hovered = FALSE;
            comp->data.button.pressed = FALSE;
            break;
            
        case SDK_COMPONENT_TEXT_FIELD:
            comp->h = SDK_TEXTFIELD_DEFAULT_HEIGHT;
            comp->data.textfield.text[0] = L'\0';
            comp->data.textfield.placeholder[0] = L'\0';
            comp->data.textfield.text_len = 0;
            comp->data.textfield.cursor_pos = 0;
            comp->data.textfield.max_len = SDK_TEXTFIELD_MAX_CHARS;
            comp->data.textfield.color_bg = SDK_TEXTFIELD_COLOR_BG;
            comp->data.textfield.color_border = SDK_TEXTFIELD_COLOR_BORDER;
            comp->data.textfield.color_text = SDK_TEXTFIELD_COLOR_TEXT;
            break;
            
        case SDK_COMPONENT_SLIDER:
            comp->h = SDK_SLIDER_DEFAULT_HEIGHT;
            comp->data.slider.min = 0;
            comp->data.slider.max = 100;
            comp->data.slider.value = 50;
            comp->data.slider.horizontal = TRUE;
            comp->data.slider.dragging = FALSE;
            break;
            
        case SDK_COMPONENT_LIST:
            comp->data.list.item_count = 0;
            comp->data.list.selected_index = -1;
            comp->data.list.scroll_offset = 0;
            break;
            
        case SDK_COMPONENT_CHECKBOX:
            comp->data.checkbox.label[0] = L'\0';
            comp->data.checkbox.checked = FALSE;
            break;
            
        case SDK_COMPONENT_RADIO:
            comp->data.radio.option_count = 0;
            comp->data.radio.selected_id = 0;
            break;
            
        case SDK_COMPONENT_LABEL:
            comp->data.label.text[0] = L'\0';
            comp->data.label.fg_color = SDK_COLOR_TEXT_PRIMARY;
            comp->data.label.bg_color = SDK_COLOR_TRANSPARENT;
            comp->data.label.alignment = 0;  // Left
            break;
            
        case SDK_COMPONENT_PANEL:
            comp->data.panel.bg_color = SDK_COLOR_BG_LIGHT;
            comp->data.panel.border_thickness = 1;
            comp->data.panel.border_color = SDK_COLOR_BORDER;
            break;
            
        case SDK_COMPONENT_DIALOG:
            comp->data.dialog.title[0] = L'\0';
            comp->data.dialog.message[0] = L'\0';
            comp->data.dialog.button_count = 0;
            comp->data.dialog.selected_button = -1;
            comp->data.dialog.shown = FALSE;
            break;
    }
    
    return comp;
}

void sdk_component_destroy(sdk_component_t *comp) {
    if (comp == NULL) {
        return;
    }
    heap_free((void *)(UINTN)comp);
}

// ============================================================================
// Component Positioning & Sizing
// ============================================================================

void sdk_component_set_position(sdk_component_t *comp, INT32 x, INT32 y) {
    if (comp == NULL) {
        return;
    }
    comp->x = x;
    comp->y = y;
}

void sdk_component_set_size(sdk_component_t *comp, INT32 w, INT32 h) {
    if (comp == NULL) {
        return;
    }
    comp->w = w;
    comp->h = h;
}

void sdk_component_get_bounds(sdk_component_t *comp, INT32 *out_x, INT32 *out_y, INT32 *out_w, INT32 *out_h) {
    if (comp == NULL) {
        return;
    }
    if (out_x != NULL) *out_x = comp->x;
    if (out_y != NULL) *out_y = comp->y;
    if (out_w != NULL) *out_w = comp->w;
    if (out_h != NULL) *out_h = comp->h;
}

// ============================================================================
// Component State Management
// ============================================================================

void sdk_component_set_enabled(sdk_component_t *comp, BOOLEAN enabled) {
    if (comp == NULL) {
        return;
    }
    comp->enabled = enabled;
}

BOOLEAN sdk_component_is_enabled(sdk_component_t *comp) {
    if (comp == NULL) {
        return FALSE;
    }
    return comp->enabled;
}

void sdk_component_set_visible(sdk_component_t *comp, BOOLEAN visible) {
    if (comp == NULL) {
        return;
    }
    comp->visible = visible;
}

BOOLEAN sdk_component_is_visible(sdk_component_t *comp) {
    if (comp == NULL) {
        return FALSE;
    }
    return comp->visible;
}

void sdk_component_set_focused(sdk_component_t *comp, BOOLEAN focused) {
    if (comp == NULL) {
        return;
    }
    comp->focused = focused;
}

BOOLEAN sdk_component_is_focused(sdk_component_t *comp) {
    if (comp == NULL) {
        return FALSE;
    }
    return comp->focused;
}

// ============================================================================
// Callbacks
// ============================================================================

void sdk_component_on_click(sdk_component_t *comp, sdk_on_click_t callback) {
    if (comp == NULL) {
        return;
    }
    comp->on_click = callback;
}

void sdk_component_on_change(sdk_component_t *comp, sdk_on_change_t callback) {
    if (comp == NULL) {
        return;
    }
    comp->on_change = callback;
}

void sdk_component_on_submit(sdk_component_t *comp, sdk_on_submit_t callback) {
    if (comp == NULL) {
        return;
    }
    comp->on_submit = callback;
}

// ============================================================================
// Button Component
// ============================================================================

void sdk_button_set_label(sdk_component_t *comp, const CHAR16 *label) {
    if (comp == NULL || comp->type != SDK_COMPONENT_BUTTON || label == NULL) {
        return;
    }
    
    UINTN i = 0;
    while (label[i] != L'\0' && i < 63) {
        comp->data.button.label[i] = label[i];
        i++;
    }
    comp->data.button.label[i] = L'\0';
}

void sdk_button_set_color(sdk_component_t *comp, UINT32 normal_color, UINT32 hover_color, UINT32 pressed_color) {
    if (comp == NULL || comp->type != SDK_COMPONENT_BUTTON) {
        return;
    }
    comp->data.button.color_normal = normal_color;
    comp->data.button.color_hover = hover_color;
    comp->data.button.color_pressed = pressed_color;
}

BOOLEAN sdk_button_is_pressed(sdk_component_t *comp) {
    if (comp == NULL || comp->type != SDK_COMPONENT_BUTTON) {
        return FALSE;
    }
    return comp->data.button.pressed;
}

// ============================================================================
// Text Field Component
// ============================================================================

void sdk_textfield_set_text(sdk_component_t *comp, const CHAR16 *text) {
    if (comp == NULL || comp->type != SDK_COMPONENT_TEXT_FIELD || text == NULL) {
        return;
    }
    
    UINTN i = 0;
    while (text[i] != L'\0' && i < SDK_TEXTFIELD_MAX_CHARS - 1) {
        comp->data.textfield.text[i] = text[i];
        i++;
    }
    comp->data.textfield.text[i] = L'\0';
    comp->data.textfield.text_len = i;
    comp->data.textfield.cursor_pos = i;
}

void sdk_textfield_get_text(sdk_component_t *comp, CHAR16 *out_buffer, UINTN max_len) {
    if (comp == NULL || comp->type != SDK_COMPONENT_TEXT_FIELD || out_buffer == NULL) {
        return;
    }
    
    UINTN i = 0;
    while (i < max_len - 1 && comp->data.textfield.text[i] != L'\0') {
        out_buffer[i] = comp->data.textfield.text[i];
        i++;
    }
    out_buffer[i] = L'\0';
}

void sdk_textfield_set_placeholder(sdk_component_t *comp, const CHAR16 *placeholder) {
    if (comp == NULL || comp->type != SDK_COMPONENT_TEXT_FIELD || placeholder == NULL) {
        return;
    }
    
    UINTN i = 0;
    while (placeholder[i] != L'\0' && i < 63) {
        comp->data.textfield.placeholder[i] = placeholder[i];
        i++;
    }
    comp->data.textfield.placeholder[i] = L'\0';
}

void sdk_textfield_set_max_length(sdk_component_t *comp, UINTN max_len) {
    if (comp == NULL || comp->type != SDK_COMPONENT_TEXT_FIELD) {
        return;
    }
    comp->data.textfield.max_len = sdk_min(max_len, SDK_TEXTFIELD_MAX_CHARS - 1);
}

void sdk_textfield_clear(sdk_component_t *comp) {
    if (comp == NULL || comp->type != SDK_COMPONENT_TEXT_FIELD) {
        return;
    }
    comp->data.textfield.text[0] = L'\0';
    comp->data.textfield.text_len = 0;
    comp->data.textfield.cursor_pos = 0;
}

// ============================================================================
// Slider Component
// ============================================================================

void sdk_slider_set_range(sdk_component_t *comp, INT32 min, INT32 max) {
    if (comp == NULL || comp->type != SDK_COMPONENT_SLIDER) {
        return;
    }
    comp->data.slider.min = min;
    comp->data.slider.max = max;
    if (comp->data.slider.value < min) {
        comp->data.slider.value = min;
    }
    if (comp->data.slider.value > max) {
        comp->data.slider.value = max;
    }
}

void sdk_slider_set_value(sdk_component_t *comp, INT32 value) {
    if (comp == NULL || comp->type != SDK_COMPONENT_SLIDER) {
        return;
    }
    comp->data.slider.value = sdk_clamp(value, comp->data.slider.min, comp->data.slider.max);
}

INT32 sdk_slider_get_value(sdk_component_t *comp) {
    if (comp == NULL || comp->type != SDK_COMPONENT_SLIDER) {
        return 0;
    }
    return comp->data.slider.value;
}

void sdk_slider_set_orientation(sdk_component_t *comp, BOOLEAN horizontal) {
    if (comp == NULL || comp->type != SDK_COMPONENT_SLIDER) {
        return;
    }
    comp->data.slider.horizontal = horizontal;
}

// ============================================================================
// List Component
// ============================================================================

void sdk_list_add_item(sdk_component_t *comp, const sdk_list_item_t *item) {
    if (comp == NULL || comp->type != SDK_COMPONENT_LIST || item == NULL) {
        return;
    }
    if (comp->data.list.item_count >= 32) {
        return;
    }
    
    comp->data.list.items[comp->data.list.item_count] = *item;
    comp->data.list.item_count++;
}

void sdk_list_clear_items(sdk_component_t *comp) {
    if (comp == NULL || comp->type != SDK_COMPONENT_LIST) {
        return;
    }
    comp->data.list.item_count = 0;
    comp->data.list.selected_index = -1;
}

INT32 sdk_list_get_selected_index(sdk_component_t *comp) {
    if (comp == NULL || comp->type != SDK_COMPONENT_LIST) {
        return -1;
    }
    return comp->data.list.selected_index;
}

void sdk_list_set_selected_index(sdk_component_t *comp, INT32 index) {
    if (comp == NULL || comp->type != SDK_COMPONENT_LIST) {
        return;
    }
    if (index >= 0 && index < (INT32)comp->data.list.item_count) {
        comp->data.list.selected_index = index;
    }
}

BOOLEAN sdk_list_get_selected_item(sdk_component_t *comp, sdk_list_item_t *out_item) {
    if (comp == NULL || comp->type != SDK_COMPONENT_LIST || out_item == NULL) {
        return FALSE;
    }
    if (comp->data.list.selected_index < 0 || comp->data.list.selected_index >= (INT32)comp->data.list.item_count) {
        return FALSE;
    }
    *out_item = comp->data.list.items[comp->data.list.selected_index];
    return TRUE;
}

// ============================================================================
// Checkbox Component
// ============================================================================

void sdk_checkbox_set_label(sdk_component_t *comp, const CHAR16 *label) {
    if (comp == NULL || comp->type != SDK_COMPONENT_CHECKBOX || label == NULL) {
        return;
    }
    
    UINTN i = 0;
    while (label[i] != L'\0' && i < 63) {
        comp->data.checkbox.label[i] = label[i];
        i++;
    }
    comp->data.checkbox.label[i] = L'\0';
}

void sdk_checkbox_set_checked(sdk_component_t *comp, BOOLEAN checked) {
    if (comp == NULL || comp->type != SDK_COMPONENT_CHECKBOX) {
        return;
    }
    comp->data.checkbox.checked = checked;
}

BOOLEAN sdk_checkbox_is_checked(sdk_component_t *comp) {
    if (comp == NULL || comp->type != SDK_COMPONENT_CHECKBOX) {
        return FALSE;
    }
    return comp->data.checkbox.checked;
}

// ============================================================================
// Radio Component
// ============================================================================

void sdk_radio_add_option(sdk_component_t *comp, const sdk_radio_option_t *option) {
    if (comp == NULL || comp->type != SDK_COMPONENT_RADIO || option == NULL) {
        return;
    }
    if (comp->data.radio.option_count >= 16) {
        return;
    }
    
    comp->data.radio.options[comp->data.radio.option_count] = *option;
    comp->data.radio.option_count++;
}

void sdk_radio_set_selected(sdk_component_t *comp, UINT32 option_id) {
    if (comp == NULL || comp->type != SDK_COMPONENT_RADIO) {
        return;
    }
    comp->data.radio.selected_id = option_id;
}

UINT32 sdk_radio_get_selected(sdk_component_t *comp) {
    if (comp == NULL || comp->type != SDK_COMPONENT_RADIO) {
        return 0;
    }
    return comp->data.radio.selected_id;
}

// ============================================================================
// Label Component
// ============================================================================

void sdk_label_set_text(sdk_component_t *comp, const CHAR16 *text) {
    if (comp == NULL || comp->type != SDK_COMPONENT_LABEL || text == NULL) {
        return;
    }
    
    UINTN i = 0;
    while (text[i] != L'\0' && i < 255) {
        comp->data.label.text[i] = text[i];
        i++;
    }
    comp->data.label.text[i] = L'\0';
}

void sdk_label_set_color(sdk_component_t *comp, UINT32 fg, UINT32 bg) {
    if (comp == NULL || comp->type != SDK_COMPONENT_LABEL) {
        return;
    }
    comp->data.label.fg_color = fg;
    comp->data.label.bg_color = bg;
}

void sdk_label_set_alignment(sdk_component_t *comp, INT32 align) {
    if (comp == NULL || comp->type != SDK_COMPONENT_LABEL) {
        return;
    }
    comp->data.label.alignment = sdk_clamp(align, 0, 2);
}

// ============================================================================
// Panel Component
// ============================================================================

void sdk_panel_set_color(sdk_component_t *comp, UINT32 color) {
    if (comp == NULL || comp->type != SDK_COMPONENT_PANEL) {
        return;
    }
    comp->data.panel.bg_color = color;
}

void sdk_panel_set_border(sdk_component_t *comp, INT32 thickness, UINT32 color) {
    if (comp == NULL || comp->type != SDK_COMPONENT_PANEL) {
        return;
    }
    comp->data.panel.border_thickness = thickness;
    comp->data.panel.border_color = color;
}

// ============================================================================
// Dialog Component
// ============================================================================

void sdk_dialog_set_title(sdk_component_t *comp, const CHAR16 *title) {
    if (comp == NULL || comp->type != SDK_COMPONENT_DIALOG || title == NULL) {
        return;
    }
    
    UINTN i = 0;
    while (title[i] != L'\0' && i < 63) {
        comp->data.dialog.title[i] = title[i];
        i++;
    }
    comp->data.dialog.title[i] = L'\0';
}

void sdk_dialog_set_message(sdk_component_t *comp, const CHAR16 *message) {
    if (comp == NULL || comp->type != SDK_COMPONENT_DIALOG || message == NULL) {
        return;
    }
    
    UINTN i = 0;
    while (message[i] != L'\0' && i < 511) {
        comp->data.dialog.message[i] = message[i];
        i++;
    }
    comp->data.dialog.message[i] = L'\0';
}

void sdk_dialog_add_button(sdk_component_t *comp, const CHAR16 *label, INT32 button_id) {
    if (comp == NULL || comp->type != SDK_COMPONENT_DIALOG || label == NULL) {
        return;
    }
    if (comp->data.dialog.button_count >= 8) {
        return;
    }
    
    UINTN i = 0;
    while (label[i] != L'\0' && i < 63) {
        comp->data.dialog.buttons[comp->data.dialog.button_count].label[i] = label[i];
        i++;
    }
    comp->data.dialog.buttons[comp->data.dialog.button_count].label[i] = L'\0';
    comp->data.dialog.buttons[comp->data.dialog.button_count].button_id = button_id;
    comp->data.dialog.button_count++;
}

INT32 sdk_dialog_get_selected_button(sdk_component_t *comp) {
    if (comp == NULL || comp->type != SDK_COMPONENT_DIALOG) {
        return -1;
    }
    return comp->data.dialog.selected_button;
}

BOOLEAN sdk_dialog_is_shown(sdk_component_t *comp) {
    if (comp == NULL || comp->type != SDK_COMPONENT_DIALOG) {
        return FALSE;
    }
    return comp->data.dialog.shown;
}

void sdk_dialog_show(sdk_component_t *comp) {
    if (comp == NULL || comp->type != SDK_COMPONENT_DIALOG) {
        return;
    }
    comp->data.dialog.shown = TRUE;
}

void sdk_dialog_hide(sdk_component_t *comp) {
    if (comp == NULL || comp->type != SDK_COMPONENT_DIALOG) {
        return;
    }
    comp->data.dialog.shown = FALSE;
}

// ============================================================================
// Component Rendering & Input (Stubs - to be expanded)
// ============================================================================

void sdk_component_render(sdk_component_t *comp) {
    if (comp == NULL || !comp->visible) {
        return;
    }
    
    switch (comp->type) {
        case SDK_COMPONENT_BUTTON: {
            UINT32 color = comp->data.button.color_normal;
            if (comp->data.button.pressed) {
                color = comp->data.button.color_pressed;
            } else if (comp->data.button.hovered) {
                color = comp->data.button.color_hover;
            }
            
            sdk_graphics_rect(comp->x, comp->y, comp->w, comp->h, color);
            sdk_graphics_text(comp->x + 4, comp->y + 8, comp->data.button.label, SDK_BUTTON_COLOR_TEXT, color);
            break;
        }
        
        case SDK_COMPONENT_TEXT_FIELD: {
            sdk_graphics_rect(comp->x, comp->y, comp->w, comp->h, comp->data.textfield.color_bg);
            sdk_graphics_rect_outline(comp->x, comp->y, comp->w, comp->h, comp->data.textfield.color_border, 1);
            
            CHAR16 *display_text = comp->data.textfield.text_len > 0 ? 
                comp->data.textfield.text : comp->data.textfield.placeholder;
            UINT32 text_color = comp->data.textfield.text_len > 0 ? 
                comp->data.textfield.color_text : SDK_COLOR_LIGHT_GRAY;
            
            sdk_graphics_text(comp->x + 4, comp->y + 8, display_text, text_color, comp->data.textfield.color_bg);
            
            // Draw cursor if focused
            if (comp->focused) {
                INT32 cursor_x = comp->x + 4 + (comp->data.textfield.cursor_pos * SDK_FONT_WIDTH);
                sdk_graphics_line(cursor_x, comp->y + 4, cursor_x, comp->y + comp->h - 4, SDK_COLOR_TEXT_PRIMARY);
            }
            break;
        }
        
        case SDK_COMPONENT_SLIDER: {
            // Draw track
            INT32 track_y = comp->y + comp->h / 2 - 2;
            sdk_graphics_rect(comp->x, track_y, comp->w, 4, SDK_SLIDER_TRACK_COLOR);
            
            // Draw thumb
            INT32 range = comp->data.slider.max - comp->data.slider.min;
            INT32 thumb_x = comp->x + (comp->w * (comp->data.slider.value - comp->data.slider.min)) / sdk_max(range, 1);
            sdk_graphics_circle(thumb_x, comp->y + comp->h / 2, SDK_SLIDER_THUMB_RADIUS, SDK_SLIDER_THUMB_COLOR);
            break;
        }
        
        case SDK_COMPONENT_LIST: {
            sdk_graphics_rect(comp->x, comp->y, comp->w, comp->h, SDK_COLOR_BG_LIGHT);
            sdk_graphics_rect_outline(comp->x, comp->y, comp->w, comp->h, SDK_COLOR_BORDER, 1);
            
            INT32 item_y = comp->y;
            for (UINTN i = 0; i < comp->data.list.item_count && item_y < comp->y + comp->h; i++) {
                if ((INT32)i == comp->data.list.selected_index) {
                    sdk_graphics_rect(comp->x, item_y, comp->w, SDK_LIST_ITEM_HEIGHT, SDK_LIST_HIGHLIGHT_COLOR);
                    sdk_graphics_text(comp->x + 4, item_y + 4, comp->data.list.items[i].label, SDK_COLOR_WHITE, SDK_LIST_HIGHLIGHT_COLOR);
                } else {
                    sdk_graphics_text(comp->x + 4, item_y + 4, comp->data.list.items[i].label, SDK_COLOR_TEXT_PRIMARY, SDK_COLOR_BG_LIGHT);
                }
                item_y += SDK_LIST_ITEM_HEIGHT;
            }
            break;
        }
        
        case SDK_COMPONENT_LABEL: {
            sdk_graphics_text(comp->x, comp->y, comp->data.label.text, comp->data.label.fg_color, comp->data.label.bg_color);
            break;
        }
        
        case SDK_COMPONENT_PANEL: {
            sdk_graphics_rect(comp->x, comp->y, comp->w, comp->h, comp->data.panel.bg_color);
            if (comp->data.panel.border_thickness > 0) {
                sdk_graphics_rect_outline(comp->x, comp->y, comp->w, comp->h, comp->data.panel.border_color, comp->data.panel.border_thickness);
            }
            break;
        }
        
        case SDK_COMPONENT_CHECKBOX: {
            // Draw checkbox box
            sdk_graphics_rect_outline(comp->x, comp->y, 16, 16, SDK_COLOR_TEXT_PRIMARY, 1);
            if (comp->data.checkbox.checked) {
                sdk_graphics_rect(comp->x + 2, comp->y + 2, 12, 12, SDK_BUTTON_COLOR_NORMAL);
            }
            // Draw label
            sdk_graphics_text(comp->x + 20, comp->y, comp->data.checkbox.label, SDK_COLOR_TEXT_PRIMARY, SDK_COLOR_TRANSPARENT);
            break;
        }
        
        case SDK_COMPONENT_RADIO: {
            INT32 radio_y = comp->y;
            for (UINTN i = 0; i < comp->data.radio.option_count; i++) {
                // Draw radio button circle
                sdk_graphics_circle(comp->x + 8, radio_y + 8, 6, SDK_COLOR_TEXT_PRIMARY);
                if (comp->data.radio.options[i].id == comp->data.radio.selected_id) {
                    sdk_graphics_circle(comp->x + 8, radio_y + 8, 3, SDK_BUTTON_COLOR_NORMAL);
                }
                // Draw label
                sdk_graphics_text(comp->x + 20, radio_y, comp->data.radio.options[i].label, SDK_COLOR_TEXT_PRIMARY, SDK_COLOR_TRANSPARENT);
                radio_y += 20;
            }
            break;
        }
        
        case SDK_COMPONENT_DIALOG: {
            if (!comp->data.dialog.shown) {
                break;
            }
            
            // Draw semi-transparent overlay
            sdk_graphics_rect(0, 0, 1024, 768, 0x80000000);
            
            // Draw dialog box
            INT32 dialog_x = 200;
            INT32 dialog_y = 150;
            INT32 dialog_w = 624;
            INT32 dialog_h = 468;
            
            sdk_graphics_rect(dialog_x, dialog_y, dialog_w, dialog_h, SDK_COLOR_WHITE);
            sdk_graphics_rect_outline(dialog_x, dialog_y, dialog_w, dialog_h, SDK_COLOR_BORDER, 2);
            
            // Draw title
            sdk_graphics_text(dialog_x + 16, dialog_y + 12, comp->data.dialog.title, SDK_COLOR_TEXT_PRIMARY, SDK_COLOR_WHITE);
            
            // Draw message
            sdk_graphics_text(dialog_x + 16, dialog_y + 60, comp->data.dialog.message, SDK_COLOR_TEXT_PRIMARY, SDK_COLOR_WHITE);
            
            // Draw buttons
            INT32 button_x = dialog_x + 16;
            for (UINTN i = 0; i < comp->data.dialog.button_count; i++) {
                UINT32 btn_color = (i == (UINTN)comp->data.dialog.selected_button) ? 
                    SDK_BUTTON_COLOR_HOVER : SDK_BUTTON_COLOR_NORMAL;
                sdk_graphics_rect(button_x, dialog_y + dialog_h - 60, 120, 40, btn_color);
                sdk_graphics_rect_outline(button_x, dialog_y + dialog_h - 60, 120, 40, SDK_COLOR_BORDER, 1);
                sdk_graphics_text(button_x + 8, dialog_y + dialog_h - 48, comp->data.dialog.buttons[i].label, SDK_COLOR_WHITE, btn_color);
                button_x += 140;
            }
            break;
        }
        
        default:
            break;
    }
}

BOOLEAN sdk_component_handle_input(sdk_component_t *comp, const input_event_t *event) {
    if (comp == NULL || !comp->enabled || event == NULL) {
        return FALSE;
    }
    
    switch (comp->type) {
        case SDK_COMPONENT_BUTTON: {
            if (sdk_input_is_left_click(event) && sdk_input_hit_test_rect(event, comp->x, comp->y, comp->w, comp->h)) {
                comp->data.button.pressed = TRUE;
                if (comp->on_click != NULL) {
                    comp->on_click(comp);
                }
                return TRUE;
            }
            if (sdk_input_is_mouse_release(event)) {
                comp->data.button.pressed = FALSE;
            }
            if (sdk_input_hit_test_rect(event, comp->x, comp->y, comp->w, comp->h) && sdk_input_is_mouse_move(event)) {
                comp->data.button.hovered = TRUE;
            } else {
                comp->data.button.hovered = FALSE;
            }
            break;
        }
        
        case SDK_COMPONENT_TEXT_FIELD: {
            if (comp->focused) {
                if (sdk_input_is_printable(event)) {
                    CHAR16 ch;
                    if (sdk_input_get_char(event, &ch)) {
                        if (comp->data.textfield.text_len < comp->data.textfield.max_len) {
                            comp->data.textfield.text[comp->data.textfield.cursor_pos] = ch;
                            comp->data.textfield.cursor_pos++;
                            comp->data.textfield.text_len++;
                            comp->data.textfield.text[comp->data.textfield.text_len] = L'\0';
                            if (comp->on_change != NULL) {
                                comp->on_change(comp);
                            }
                            return TRUE;
                        }
                    }
                } else if (sdk_input_is_backspace(event)) {
                    if (comp->data.textfield.cursor_pos > 0) {
                        comp->data.textfield.cursor_pos--;
                        comp->data.textfield.text_len--;
                        comp->data.textfield.text[comp->data.textfield.text_len] = L'\0';
                        if (comp->on_change != NULL) {
                            comp->on_change(comp);
                        }
                        return TRUE;
                    }
                } else if (sdk_input_is_enter(event)) {
                    if (comp->on_submit != NULL) {
                        comp->on_submit(comp);
                    }
                    return TRUE;
                }
            }
            
            if (sdk_input_is_left_click(event) && sdk_input_hit_test_rect(event, comp->x, comp->y, comp->w, comp->h)) {
                comp->focused = TRUE;
                return TRUE;
            }
            break;
        }
        
        case SDK_COMPONENT_SLIDER: {
            if (sdk_input_is_left_click(event)) {
                comp->data.slider.dragging = TRUE;
            }
            if (sdk_input_is_mouse_release(event)) {
                comp->data.slider.dragging = FALSE;
            }
            if (comp->data.slider.dragging && sdk_input_is_mouse_move(event)) {
                INT32 mx, my;
                if (sdk_input_get_mouse_pos(event, &mx, &my)) {
                    INT32 relative_x = mx - comp->x;
                    INT32 range = comp->data.slider.max - comp->data.slider.min;
                    INT32 new_value = comp->data.slider.min + (relative_x * range) / comp->w;
                    sdk_slider_set_value(comp, new_value);
                    if (comp->on_change != NULL) {
                        comp->on_change(comp);
                    }
                    return TRUE;
                }
            }
            break;
        }
        
        case SDK_COMPONENT_LIST: {
            if (sdk_input_is_left_click(event)) {
                INT32 mx, my;
                if (sdk_input_get_mouse_pos(event, &mx, &my)) {
                    INT32 item_y = comp->y;
                    for (UINTN i = 0; i < comp->data.list.item_count; i++) {
                        if (my >= item_y && my < item_y + SDK_LIST_ITEM_HEIGHT) {
                            comp->data.list.selected_index = i;
                            if (comp->on_click != NULL) {
                                comp->on_click(comp);
                            }
                            return TRUE;
                        }
                        item_y += SDK_LIST_ITEM_HEIGHT;
                    }
                }
            }
            break;
        }
        
         case SDK_COMPONENT_CHECKBOX: {
            if (sdk_input_is_left_click(event) && sdk_input_hit_test_rect(event, comp->x, comp->y, 16, 16)) {
                comp->data.checkbox.checked = !comp->data.checkbox.checked;
                if (comp->on_change != NULL) {
                    comp->on_change(comp);
                }
                return TRUE;
            }
            break;
        }
        
        case SDK_COMPONENT_RADIO: {
            if (sdk_input_is_left_click(event)) {
                INT32 mx, my;
                if (sdk_input_get_mouse_pos(event, &mx, &my)) {
                    INT32 radio_y = comp->y;
                    for (UINTN i = 0; i < comp->data.radio.option_count; i++) {
                        if (my >= radio_y && my < radio_y + 20 && mx >= comp->x && mx < comp->x + comp->w) {
                            comp->data.radio.selected_id = comp->data.radio.options[i].id;
                            if (comp->on_change != NULL) {
                                comp->on_change(comp);
                            }
                            return TRUE;
                        }
                        radio_y += 20;
                    }
                }
            }
            break;
        }
        
        case SDK_COMPONENT_DIALOG: {
            if (!comp->data.dialog.shown) {
                break;
            }
            
            if (sdk_input_is_left_click(event)) {
                INT32 mx, my;
                if (sdk_input_get_mouse_pos(event, &mx, &my)) {
                    INT32 dialog_x = 200;
                    INT32 dialog_y = 150;
                    INT32 dialog_h = 468;
                    
                    INT32 button_x = dialog_x + 16;
                    for (UINTN i = 0; i < comp->data.dialog.button_count; i++) {
                        if (mx >= button_x && mx < button_x + 120 && 
                            my >= dialog_y + dialog_h - 60 && my < dialog_y + dialog_h - 20) {
                            if (comp->on_click != NULL) {
                                comp->on_click(comp);
                            }
                            comp->data.dialog.selected_button = i;
                            return TRUE;
                        }
                        button_x += 140;
                    }
                }
            }
            break;
        }
        
        default:
            break;
    }
    
    return FALSE;
}
