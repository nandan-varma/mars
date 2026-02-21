// ============================================================================
// File Manager Application
// ============================================================================
// Demonstrates List component usage with file browsing simulation
// Uses the SDK layout system and app framework

#include "app.h"
#include "sdk/sdk_app.h"
#include "sdk/sdk_layout.h"
#include "sdk/sdk_ui.h"
#include "sdk/sdk_graphics.h"

// File manager state
typedef struct {
    CHAR16 current_path[256];
    UINTN file_count;
    struct {
        CHAR16 name[64];
        BOOLEAN is_directory;
        UINT32 size;
    } files[32];
} file_manager_state_t;

// Initialize file list with sample files
static void file_manager_init_files(file_manager_state_t *state) {
    state->file_count = 0;
    
    // Add some sample files
    os_string_copy_chars(state->files[state->file_count].name, 64, L"[..]");
    state->files[state->file_count].is_directory = TRUE;
    state->files[state->file_count].size = 0;
    state->file_count++;
    
    os_string_copy_chars(state->files[state->file_count].name, 64, L"Documents");
    state->files[state->file_count].is_directory = TRUE;
    state->files[state->file_count].size = 0;
    state->file_count++;
    
    os_string_copy_chars(state->files[state->file_count].name, 64, L"Photos");
    state->files[state->file_count].is_directory = TRUE;
    state->files[state->file_count].size = 0;
    state->file_count++;
    
    os_string_copy_chars(state->files[state->file_count].name, 64, L"readme.txt");
    state->files[state->file_count].is_directory = FALSE;
    state->files[state->file_count].size = 1024;
    state->file_count++;
    
    os_string_copy_chars(state->files[state->file_count].name, 64, L"config.sys");
    state->files[state->file_count].is_directory = FALSE;
    state->files[state->file_count].size = 512;
    state->file_count++;
    
    os_string_copy_chars(state->files[state->file_count].name, 64, L"data.bin");
    state->files[state->file_count].is_directory = FALSE;
    state->files[state->file_count].size = 4096;
    state->file_count++;
}

// Application initialization
static void file_manager_on_init(sdk_app_t *app) {
    file_manager_state_t *state = (file_manager_state_t*)heap_alloc(sizeof(file_manager_state_t));
    if (state == NULL) {
        return;
    }
    
    os_string_copy_chars(state->current_path, 256, L"FS0:\\");
    file_manager_init_files(state);
    sdk_app_set_user_data(app, state);
    
    // Get root container and set layout
    sdk_container_t *container = sdk_app_get_container(app);
    if (container == NULL) {
        return;
    }
    
    sdk_container_set_layout(container, SDK_LAYOUT_VERTICAL, 10, 10);
    
    // Title label
    sdk_component_t *title = sdk_label_create(10, 10, 1004, 32);
    if (title != NULL) {
        sdk_label_set_text(title, L"File Manager");
        sdk_label_set_colors(title, SDK_COLOR_WHITE, SDK_BUTTON_COLOR_NORMAL);
        sdk_container_add_component(container, title);
    }
    
    // Path label
    sdk_component_t *path_label = sdk_label_create(10, 50, 1004, 24);
    if (path_label != NULL) {
        sdk_label_set_text(path_label, state->current_path);
        sdk_container_add_component(container, path_label);
    }
    
    // File list
    sdk_component_t *file_list = sdk_list_create(10, 80, 1004, 600);
    if (file_list != NULL) {
        for (UINTN i = 0; i < state->file_count && i < 32; i++) {
            sdk_list_item_t item;
            os_string_copy_chars(item.label, 64, state->files[i].name);
            item.selectable = TRUE;
            sdk_list_add_item(file_list, item);
        }
        sdk_container_add_component(container, file_list);
    }
    
    // Button bar
    sdk_component_t *open_btn = sdk_button_create(10, 690, 80, 32);
    if (open_btn != NULL) {
        sdk_button_set_label(open_btn, L"Open");
        sdk_container_add_component(container, open_btn);
    }
    
    sdk_component_t *back_btn = sdk_button_create(100, 690, 80, 32);
    if (back_btn != NULL) {
        sdk_button_set_label(back_btn, L"Back");
        sdk_container_add_component(container, back_btn);
    }
    
    sdk_component_t *refresh_btn = sdk_button_create(190, 690, 80, 32);
    if (refresh_btn != NULL) {
        sdk_button_set_label(refresh_btn, L"Refresh");
        sdk_container_add_component(container, refresh_btn);
    }
}

// Application update
static void file_manager_on_update(sdk_app_t *app) {
    // File manager would handle user input here
    // For now, just maintain the display
}

// Application cleanup
static void file_manager_on_cleanup(sdk_app_t *app) {
    file_manager_state_t *state = (file_manager_state_t*)sdk_app_get_user_data(app);
    if (state != NULL) {
        heap_free(state);
    }
}

// Main entry point for file manager
EFI_STATUS EFIAPI file_manager_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    // Create application
    sdk_app_t *app = sdk_app_create(L"File Manager", 1024, 768);
    if (app == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }
    
    // Set lifecycle callbacks
    sdk_app_set_lifecycle(app, 
                         file_manager_on_init,
                         file_manager_on_update,
                         file_manager_on_cleanup);
    
    // Set background color
    sdk_app_set_background(app, SDK_COLOR_BG_LIGHT);
    
    // Run application
    while (sdk_app_update(app)) {
        // Application main loop
    }
    
    // Cleanup
    sdk_app_destroy(app);
    
    return EFI_SUCCESS;
}
