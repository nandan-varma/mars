#pragma once

#include "sdk_layout.h"
#include "sdk_ui.h"
#include "input.h"
#include "uefi.h"

// ============================================================================
// Application Framework - Layer 6
// ============================================================================
// Provides lifecycle management and application templates for SDK apps.
// Handles app initialization, frame updates, input, and cleanup.
//
// Typical usage:
//   sdk_app_t *app = sdk_app_create(L"My App", 1024, 768);
//   sdk_app_set_lifecycle(app, on_init, on_update, on_cleanup);
//   while (app_is_running) {
//       sdk_app_update(app);
//   }
//   sdk_app_destroy(app);

// ============================================================================
// Type Definitions
// ============================================================================

// Opaque app handle
typedef struct sdk_app sdk_app_t;

// Callback signatures for app lifecycle
typedef void (*sdk_app_init_t)(sdk_app_t *app);
typedef void (*sdk_app_update_t)(sdk_app_t *app);
typedef void (*sdk_app_cleanup_t)(sdk_app_t *app);

// Application state enumeration
typedef enum {
    SDK_APP_STATE_INIT,      // Initialization phase
    SDK_APP_STATE_RUNNING,   // Running normally
    SDK_APP_STATE_PAUSED,    // Paused (window minimized, etc)
    SDK_APP_STATE_SHUTDOWN,  // Shutting down
} sdk_app_state_t;

// ============================================================================
// App Lifecycle
// ============================================================================

// Create a new application with given title and default window size
// Returns NULL on allocation failure
sdk_app_t* sdk_app_create(const CHAR16 *title, INT32 width, INT32 height);

// Destroy application and free all resources
void sdk_app_destroy(sdk_app_t *app);

// Set lifecycle callbacks
// Callbacks are optional (can be NULL)
void sdk_app_set_lifecycle(sdk_app_t *app,
                          sdk_app_init_t on_init,
                          sdk_app_update_t on_update,
                          sdk_app_cleanup_t on_cleanup);

// ============================================================================
// App Execution
// ============================================================================

// Update app for one frame
// Processes input, calls update callback, renders
// Returns FALSE if app should exit
BOOLEAN sdk_app_update(sdk_app_t *app);

// Get current application state
sdk_app_state_t sdk_app_get_state(sdk_app_t *app);

// Set application state
void sdk_app_set_state(sdk_app_t *app, sdk_app_state_t state);

// Request app shutdown (graceful exit)
void sdk_app_request_shutdown(sdk_app_t *app);

// Check if app is still running
BOOLEAN sdk_app_is_running(sdk_app_t *app);

// ============================================================================
// App Properties
// ============================================================================

// Get app title
const CHAR16* sdk_app_get_title(sdk_app_t *app);

// Set app title
void sdk_app_set_title(sdk_app_t *app, const CHAR16 *title);

// Get app window size
void sdk_app_get_size(sdk_app_t *app, INT32 *width, INT32 *height);

// Get app window position
void sdk_app_get_position(sdk_app_t *app, INT32 *x, INT32 *y);

// ============================================================================
// Container and Component Management
// ============================================================================

// Get the root container for the app
// Add components to this container
sdk_container_t* sdk_app_get_container(sdk_app_t *app);

// Set background color for app window
void sdk_app_set_background(sdk_app_t *app, UINT32 color);

// ============================================================================
// Rendering
// ============================================================================

// Request screen refresh (forces redraw next frame)
void sdk_app_request_refresh(sdk_app_t *app);

// Get if a refresh is pending
BOOLEAN sdk_app_has_pending_refresh(sdk_app_t *app);

// ============================================================================
// User Data Storage
// ============================================================================

// Store opaque user data pointer in app
// Useful for app-specific state
void sdk_app_set_user_data(sdk_app_t *app, void *user_data);

// Retrieve user data pointer
void* sdk_app_get_user_data(sdk_app_t *app);

// ============================================================================
// Advanced: Frame Counter and Timing
// ============================================================================

// Get current frame number (increments each update)
UINT32 sdk_app_get_frame_count(sdk_app_t *app);

// Get frame time (milliseconds since last frame)
// Note: Approximate timing, not guaranteed precision
UINT32 sdk_app_get_frame_time_ms(sdk_app_t *app);

// ============================================================================
// Built-in App Templates
// ============================================================================

// Template for simple list-based app (file browser, etc)
typedef struct {
    CHAR16 items[16][64];
    UINTN item_count;
    INT32 selected_index;
} sdk_list_app_state_t;

// Initialize a list app template
sdk_app_t* sdk_app_template_list(const CHAR16 *title,
                                const CHAR16 **item_names,
                                UINTN item_count);

// Template for form-based app (settings, dialog, etc)
typedef struct {
    // Up to 8 form fields
    struct {
        CHAR16 label[32];
        CHAR16 value[256];
    } fields[8];
    UINTN field_count;
} sdk_form_app_state_t;

// Initialize a form app template
sdk_app_t* sdk_app_template_form(const CHAR16 *title,
                                const CHAR16 **field_labels,
                                UINTN field_count);
