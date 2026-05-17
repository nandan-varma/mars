// ============================================================================
// File Manager Application
// ============================================================================
// Demonstrates file browsing with list of sample files

#include "uefi.h"
#include "heap.h"
#include "sdk/sdk_core.h"
#include "sdk/sdk_graphics.h"
#include "sdk/sdk_input.h"
#include "sdk/sdk_ui.h"

// File manager state
typedef struct {
    CHAR16 current_path[256];
    UINTN file_count;
    INT32 selected_index;
    struct {
        CHAR16 name[64];
        BOOLEAN is_directory;
        UINT32 size;
    } files[32];
} file_manager_state_t;

static file_manager_state_t *g_fm_state = NULL;

// Simple string copy helper
static void copy_str16(CHAR16 *dst, const CHAR16 *src) {
    UINTN i = 0;
    while (src[i] != 0 && i < 63) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
}

// Initialize file list with sample files
static void file_manager_init_files(file_manager_state_t *state) {
    state->file_count = 0;
    state->selected_index = 0;
    
    // Add some sample files
    if (state->file_count < 32) {
        copy_str16(state->files[state->file_count].name, L"[..]");
        state->files[state->file_count].is_directory = TRUE;
        state->files[state->file_count].size = 0;
        state->file_count++;
    }
    
    if (state->file_count < 32) {
        copy_str16(state->files[state->file_count].name, L"Documents");
        state->files[state->file_count].is_directory = TRUE;
        state->files[state->file_count].size = 0;
        state->file_count++;
    }
    
    if (state->file_count < 32) {
        copy_str16(state->files[state->file_count].name, L"Photos");
        state->files[state->file_count].is_directory = TRUE;
        state->files[state->file_count].size = 0;
        state->file_count++;
    }
    
    if (state->file_count < 32) {
        copy_str16(state->files[state->file_count].name, L"readme.txt");
        state->files[state->file_count].is_directory = FALSE;
        state->files[state->file_count].size = 1024;
        state->file_count++;
    }
    
    if (state->file_count < 32) {
        copy_str16(state->files[state->file_count].name, L"config.sys");
        state->files[state->file_count].is_directory = FALSE;
        state->files[state->file_count].size = 512;
        state->file_count++;
    }
    
    if (state->file_count < 32) {
        copy_str16(state->files[state->file_count].name, L"data.bin");
        state->files[state->file_count].is_directory = FALSE;
        state->files[state->file_count].size = 4096;
        state->file_count++;
    }
}

BOOLEAN file_manager_init(UINT32 window_id) {
    (void)window_id;
    g_fm_state = (file_manager_state_t *)heap_alloc(sizeof(file_manager_state_t));
    if (g_fm_state == NULL) {
        return FALSE;
    }
    
    copy_str16(g_fm_state->current_path, L"FS0:\\");
    file_manager_init_files(g_fm_state);
    
    return TRUE;
}

void file_manager_render(void) {
    if (g_fm_state == NULL) {
        return;
    }
    
    sdk_graphics_clear(SDK_COLOR_BG_LIGHT);
    sdk_graphics_text(10, 10, L"File Manager", SDK_COLOR_TEXT_PRIMARY, SDK_COLOR_BG_LIGHT);
    
    // Draw current path
    sdk_graphics_text(10, 35, L"Path: ", SDK_COLOR_TEXT_SECONDARY, SDK_COLOR_BG_LIGHT);
    sdk_graphics_text(70, 35, g_fm_state->current_path, SDK_COLOR_TEXT_PRIMARY, SDK_COLOR_BG_LIGHT);
    
    // Draw file list
    INT32 y = 65;
    for (UINTN i = 0; i < g_fm_state->file_count && i < 32; i++) {
        INT32 selected = (INT32)i;
        UINT32 bg_color = (selected == g_fm_state->selected_index) ? SDK_COLOR_TEXT_PRIMARY : SDK_COLOR_BG_LIGHT;
        UINT32 fg_color = (selected == g_fm_state->selected_index) ? SDK_COLOR_BG_LIGHT : SDK_COLOR_TEXT_PRIMARY;
        
        // Draw selection background
        if (selected == g_fm_state->selected_index) {
            sdk_graphics_rect(10, y - 2, 500, 18, bg_color);
        }
        
        // Draw folder icon for directories
        if (g_fm_state->files[i].is_directory) {
            sdk_graphics_text(15, y, L"[DIR]", fg_color, bg_color);
            sdk_graphics_text(80, y, g_fm_state->files[i].name, fg_color, bg_color);
        } else {
            // Draw size for files
            sdk_graphics_text(15, y, L"[FILE]", fg_color, bg_color);
            sdk_graphics_text(90, y, g_fm_state->files[i].name, fg_color, bg_color);
        }
        
        y += 20;
    }
    
    // Draw instructions
    sdk_graphics_text(10, 500, L"UP/DOWN: Navigate | ENTER: Open | ESC: Back", SDK_COLOR_TEXT_SECONDARY, SDK_COLOR_BG_LIGHT);
}

void file_manager_handle_input(const input_event_t *event) {
    if (g_fm_state == NULL || event == NULL) {
        return;
    }
    
    // Check arrow keys
    if (sdk_input_is_arrow_key(event)) {
        sdk_arrow_dir_t dir = sdk_input_get_arrow(event);
        if (dir == SDK_ARROW_UP) {
            if (g_fm_state->selected_index > 0) {
                g_fm_state->selected_index--;
            }
        } else if (dir == SDK_ARROW_DOWN) {
            if (g_fm_state->selected_index < (INT32)g_fm_state->file_count - 1) {
                g_fm_state->selected_index++;
            }
        }
    }
    
    // Check enter key
    if (sdk_input_is_enter(event)) {
        if (g_fm_state->selected_index >= 0 && g_fm_state->selected_index < (INT32)g_fm_state->file_count) {
            // Handle enter to open
            if (g_fm_state->files[g_fm_state->selected_index].is_directory) {
                // Navigate into directory
                copy_str16(g_fm_state->current_path, g_fm_state->files[g_fm_state->selected_index].name);
            }
        }
    }
}

void file_manager_cleanup(void) {
    if (g_fm_state != NULL) {
        heap_free(g_fm_state);
        g_fm_state = NULL;
    }
}
