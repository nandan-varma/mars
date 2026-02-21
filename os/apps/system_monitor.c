// ============================================================================
// System Monitor Application
// ============================================================================
// Demonstrates dashboard-style display with system stats and progress bars

#include "uefi.h"
#include "sdk/sdk_core.h"
#include "sdk/sdk_graphics.h"
#include "sdk/sdk_input.h"
#include "sdk/sdk_ui.h"

// System monitor state
typedef struct {
    UINT32 total_memory;
    UINT32 used_memory;
    UINT32 cpu_usage;
    UINT32 disk_usage;
    UINT32 uptime_ticks;
} system_monitor_state_t;

static system_monitor_state_t *g_monitor_state = NULL;

// Draw a progress bar component
static void draw_progress_bar(INT32 x, INT32 y, INT32 width, INT32 height, 
                             UINT32 percent, UINT32 color) {
    // Draw border
    sdk_graphics_rect(x, y, width, height, SDK_COLOR_WHITE);
    sdk_graphics_line(x, y, x + width, y, 1, SDK_COLOR_BORDER);
    sdk_graphics_line(x, y, x, y + height, 1, SDK_COLOR_BORDER);
    sdk_graphics_line(x + width, y, x + width, y + height, 1, SDK_COLOR_BORDER);
    sdk_graphics_line(x, y + height, x + width, y + height, 1, SDK_COLOR_BORDER);
    
    // Draw filled portion
    INT32 filled_width = (width * percent) / 100;
    if (filled_width > 0) {
        sdk_graphics_rect(x + 1, y + 1, filled_width - 2, height - 2, color);
    }
}

// Format percentage string
static void format_percentage(CHAR16 *buffer, UINT32 percent) {
    INT32 i = 0;
    INT32 temp = percent;
    
    // Count digits
    INT32 digits = 1;
    while (temp >= 10) {
        digits++;
        temp /= 10;
    }
    
    // Write digits
    temp = percent;
    for (INT32 j = digits - 1; j >= 0; j--) {
        buffer[j] = L'0' + (temp % 10);
        temp /= 10;
    }
    buffer[digits] = L'%';
    buffer[digits + 1] = L'\0';
}

BOOLEAN system_monitor_init(UINT32 window_id) {
    g_monitor_state = (system_monitor_state_t *)heap_alloc(sizeof(system_monitor_state_t));
    if (g_monitor_state == NULL) {
        return FALSE;
    }
    
    g_monitor_state->total_memory = 512;
    g_monitor_state->used_memory = 256;
    g_monitor_state->cpu_usage = 45;
    g_monitor_state->disk_usage = 62;
    g_monitor_state->uptime_ticks = 0;
    
    return TRUE;
}

void system_monitor_render(void) {
    if (g_monitor_state == NULL) {
        return;
    }
    
    sdk_graphics_clear(SDK_COLOR_BG_LIGHT);
    sdk_graphics_text(10, 10, L"System Monitor", SDK_COLOR_TEXT_PRIMARY, SDK_COLOR_BG_LIGHT);
    
    // Memory section
    sdk_graphics_text(20, 40, L"Memory Usage:", SDK_COLOR_TEXT_PRIMARY, SDK_COLOR_BG_LIGHT);
    
    CHAR16 mem_info[64];
    os_strcpy16(mem_info, L"256 MB / 512 MB");
    sdk_graphics_text(20, 60, mem_info, SDK_COLOR_TEXT_SECONDARY, SDK_COLOR_BG_LIGHT);
    draw_progress_bar(20, 80, 200, 20, 50, SDK_COLOR_BUTTON_PRESS);
    
    // CPU section
    sdk_graphics_text(20, 120, L"CPU Usage:", SDK_COLOR_TEXT_PRIMARY, SDK_COLOR_BG_LIGHT);
    
    CHAR16 cpu_str[16];
    format_percentage(cpu_str, g_monitor_state->cpu_usage);
    sdk_graphics_text(20, 140, cpu_str, SDK_COLOR_TEXT_SECONDARY, SDK_COLOR_BG_LIGHT);
    draw_progress_bar(20, 160, 200, 20, g_monitor_state->cpu_usage, 0xFF6644);
    
    // Disk section
    sdk_graphics_text(20, 200, L"Disk Usage:", SDK_COLOR_TEXT_PRIMARY, SDK_COLOR_BG_LIGHT);
    
    CHAR16 disk_str[16];
    format_percentage(disk_str, g_monitor_state->disk_usage);
    sdk_graphics_text(20, 220, disk_str, SDK_COLOR_TEXT_SECONDARY, SDK_COLOR_BG_LIGHT);
    draw_progress_bar(20, 240, 200, 20, g_monitor_state->disk_usage, 0xFF9900);
    
    // Uptime
    sdk_graphics_text(20, 280, L"Uptime: ", SDK_COLOR_TEXT_PRIMARY, SDK_COLOR_BG_LIGHT);
    
    CHAR16 uptime_str[32];
    INT32 seconds = g_monitor_state->uptime_ticks / 10; // Approximate
    INT32 temp = seconds;
    INT32 digits = 1;
    if (temp == 0) digits = 1;
    else while (temp > 0) { digits++; temp /= 10; }
    
    temp = seconds;
    for (INT32 i = digits - 1; i >= 0; i--) {
        uptime_str[i] = L'0' + (temp % 10);
        temp /= 10;
    }
    uptime_str[digits] = L'\0';
    os_strcat16(uptime_str, L" seconds");
    sdk_graphics_text(120, 280, uptime_str, SDK_COLOR_TEXT_SECONDARY, SDK_COLOR_BG_LIGHT);
    
    g_monitor_state->uptime_ticks++;
    
    sdk_graphics_present();
}

void system_monitor_handle_input(const input_event_t *event) {
    // System Monitor is read-only, just for display
    (void)event;
}

void system_monitor_cleanup(void) {
    if (g_monitor_state != NULL) {
        heap_free((EFI_PHYSICAL_ADDRESS)(UINTN)g_monitor_state);
        g_monitor_state = NULL;
    }
}
