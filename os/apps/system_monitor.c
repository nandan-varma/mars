// ============================================================================
// System Monitor Application
// ============================================================================
// Demonstrates dashboard-style display with system stats and progress bars

#include "uefi.h"
#include "heap.h"
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
    sdk_graphics_rect(x, y, width, height, SDK_COLOR_BORDER);
    
    // Draw filled portion
    if (width <= 0 || percent == 0) return;
    // Use wider type to avoid INT32 overflow when multiplying
    INT64 mul = (INT64)width * (INT64)percent;
    INT32 filled_width = (INT32)(mul / 100);
    if (filled_width > 2) {
        sdk_graphics_rect(x + 1, y + 1, filled_width - 2, height - 2, color);
    }
}

BOOLEAN system_monitor_init(UINT32 window_id) {
    (void)window_id;
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
    sdk_graphics_text(20, 60, L"256 MB / 512 MB", SDK_COLOR_TEXT_SECONDARY, SDK_COLOR_BG_LIGHT);
    draw_progress_bar(20, 80, 200, 20, 50, 0xFF4444);
    
    // CPU section
    sdk_graphics_text(20, 120, L"CPU Usage: 45%", SDK_COLOR_TEXT_PRIMARY, SDK_COLOR_BG_LIGHT);
    draw_progress_bar(20, 140, 200, 20, 45, 0xFF6644);
    
    // Disk section
    sdk_graphics_text(20, 180, L"Disk Usage: 62%", SDK_COLOR_TEXT_PRIMARY, SDK_COLOR_BG_LIGHT);
    draw_progress_bar(20, 200, 200, 20, 62, 0xFF9900);
    
    // Uptime
    sdk_graphics_text(20, 240, L"Uptime: Running", SDK_COLOR_TEXT_SECONDARY, SDK_COLOR_BG_LIGHT);
    
    g_monitor_state->uptime_ticks++;
}

void system_monitor_handle_input(const input_event_t *event) {
    // System Monitor is read-only, just for display
    (void)event;
}

void system_monitor_cleanup(void) {
    if (g_monitor_state != NULL) {
        heap_free(g_monitor_state);
        g_monitor_state = NULL;
    }
}
