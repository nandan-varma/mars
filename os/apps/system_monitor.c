// ============================================================================
// System Monitor Application
// ============================================================================
// Demonstrates dashboard-style display with labels, sliders, and progress
// Uses the SDK graphics and layout systems to show system stats

#include "app.h"
#include "sdk/sdk_app.h"
#include "sdk/sdk_layout.h"
#include "sdk/sdk_ui.h"
#include "sdk/sdk_graphics.h"
#include "memory.h"

// System monitor state
typedef struct {
    UINT32 total_memory;
    UINT32 used_memory;
    UINT32 cpu_usage;
    UINT32 disk_usage;
    UINT32 uptime_seconds;
} system_monitor_state_t;

// Update system statistics
static void system_monitor_update_stats(system_monitor_state_t *state) {
    // Simulate memory usage
    state->total_memory = 512 * 1024 * 1024; // 512MB
    state->used_memory = 256 * 1024 * 1024; // 256MB
    
    // Simulate CPU usage (0-100%)
    state->cpu_usage = 45;
    
    // Simulate disk usage (0-100%)
    state->disk_usage = 62;
    
    // Simulate uptime
    state->uptime_seconds += 1;
}

// Draw a progress bar component
static void draw_progress_bar(INT32 x, INT32 y, INT32 width, INT32 height, 
                             UINT32 percent, UINT32 color) {
    // Draw background
    sdk_graphics_rect(x, y, width, height, SDK_COLOR_BG_LIGHT);
    
    // Draw border
    sdk_graphics_rect_outline(x, y, width, height, SDK_COLOR_BORDER, 1);
    
    // Draw filled portion
    INT32 filled_width = (width * percent) / 100;
    sdk_graphics_rect(x, y, filled_width, height, color);
    
    // Draw percentage text
    CHAR16 percent_text[16];
    os_string_format_uint(percent_text, 16, percent);
    os_string_append_chars(percent_text, 16, L"%");
    sdk_graphics_text(x + width + 4, y + (height - 16) / 2, percent_text, SDK_COLOR_TEXT_PRIMARY, SDK_COLOR_TRANSPARENT);
}

// Convert bytes to MB string
static void format_memory_mb(CHAR16 *buffer, UINTN size, UINT32 bytes) {
    UINT32 mb = bytes / (1024 * 1024);
    os_string_format_uint(buffer, size, mb);
    os_string_append_chars(buffer, size, L" MB");
}

// Application initialization
static void system_monitor_on_init(sdk_app_t *app) {
    system_monitor_state_t *state = (system_monitor_state_t*)heap_alloc(sizeof(system_monitor_state_t));
    if (state == NULL) {
        return;
    }
    
    state->total_memory = 512 * 1024 * 1024;
    state->used_memory = 256 * 1024 * 1024;
    state->cpu_usage = 45;
    state->disk_usage = 62;
    state->uptime_seconds = 0;
    
    sdk_app_set_user_data(app, state);
    
    // Get root container
    sdk_container_t *container = sdk_app_get_container(app);
    if (container == NULL) {
        return;
    }
    
    sdk_container_set_layout(container, SDK_LAYOUT_VERTICAL, 15, 20);
    
    // Title
    sdk_component_t *title = sdk_label_create(20, 20, 984, 40);
    if (title != NULL) {
        sdk_label_set_text(title, L"System Monitor");
        sdk_label_set_colors(title, SDK_COLOR_WHITE, SDK_BUTTON_COLOR_NORMAL);
        sdk_container_add_component(container, title);
    }
    
    // Memory section label
    sdk_component_t *mem_label = sdk_label_create(20, 70, 984, 24);
    if (mem_label != NULL) {
        sdk_label_set_text(mem_label, L"Memory Usage:");
        sdk_container_add_component(container, mem_label);
    }
    
    // Memory usage display
    sdk_component_t *mem_info = sdk_label_create(20, 100, 984, 24);
    if (mem_info != NULL) {
        sdk_label_set_text(mem_info, L"256 MB / 512 MB");
        sdk_container_add_component(container, mem_info);
    }
    
    // CPU section label
    sdk_component_t *cpu_label = sdk_label_create(20, 200, 984, 24);
    if (cpu_label != NULL) {
        sdk_label_set_text(cpu_label, L"CPU Usage:");
        sdk_container_add_component(container, cpu_label);
    }
    
    // CPU usage slider (read-only)
    sdk_component_t *cpu_slider = sdk_slider_create(20, 230, 300, 24);
    if (cpu_slider != NULL) {
        sdk_slider_set_range(cpu_slider, 0, 100);
        sdk_slider_set_value(cpu_slider, 45);
        sdk_container_add_component(container, cpu_slider);
    }
    
    // Disk section label
    sdk_component_t *disk_label = sdk_label_create(20, 320, 984, 24);
    if (disk_label != NULL) {
        sdk_label_set_text(disk_label, L"Disk Usage:");
        sdk_container_add_component(container, disk_label);
    }
    
    // Disk usage slider (read-only)
    sdk_component_t *disk_slider = sdk_slider_create(20, 350, 300, 24);
    if (disk_slider != NULL) {
        sdk_slider_set_range(disk_slider, 0, 100);
        sdk_slider_set_value(disk_slider, 62);
        sdk_container_add_component(container, disk_slider);
    }
    
    // Uptime section label
    sdk_component_t *uptime_label = sdk_label_create(20, 440, 984, 24);
    if (uptime_label != NULL) {
        sdk_label_set_text(uptime_label, L"System Uptime: 0 seconds");
        sdk_container_add_component(container, uptime_label);
    }
    
    // Refresh button
    sdk_component_t *refresh_btn = sdk_button_create(20, 680, 100, 40);
    if (refresh_btn != NULL) {
        sdk_button_set_label(refresh_btn, L"Refresh");
        sdk_container_add_component(container, refresh_btn);
    }
}

// Application update - refresh system stats display
static void system_monitor_on_update(sdk_app_t *app) {
    system_monitor_state_t *state = (system_monitor_state_t*)sdk_app_get_user_data(app);
    if (state == NULL) {
        return;
    }
    
    // Update statistics
    system_monitor_update_stats(state);
    
    // Request screen refresh every frame to show updated data
    sdk_app_request_refresh(app);
}

// Application cleanup
static void system_monitor_on_cleanup(sdk_app_t *app) {
    system_monitor_state_t *state = (system_monitor_state_t*)sdk_app_get_user_data(app);
    if (state != NULL) {
        heap_free(state);
    }
}

// Main entry point for system monitor
EFI_STATUS EFIAPI system_monitor_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    // Create application
    sdk_app_t *app = sdk_app_create(L"System Monitor", 1024, 768);
    if (app == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }
    
    // Set lifecycle callbacks
    sdk_app_set_lifecycle(app,
                         system_monitor_on_init,
                         system_monitor_on_update,
                         system_monitor_on_cleanup);
    
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
