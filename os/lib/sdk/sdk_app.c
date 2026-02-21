#include "sdk/sdk_app.h"
#include "sdk/sdk_core.h"
#include "sdk/sdk_layout.h"
#include "sdk/sdk_ui.h"
#include "sdk/sdk_graphics.h"
#include "sdk/sdk_input.h"
#include "heap.h"
#include "os_string.h"

// ============================================================================
// Internal App Structure
// ============================================================================

typedef struct sdk_app {
    // App identity
    CHAR16 title[64];
    INT32 width, height;
    INT32 x, y;
    
    // Lifecycle
    sdk_app_init_t on_init;
    sdk_app_update_t on_update;
    sdk_app_cleanup_t on_cleanup;
    
    // State
    sdk_app_state_t state;
    BOOLEAN running;
    BOOLEAN refresh_pending;
    
    // Container and rendering
    sdk_container_t *root_container;
    UINT32 bg_color;
    
    // Frame timing
    UINT32 frame_count;
    UINT32 frame_time_ms;
    
    // User data
    void *user_data;
} sdk_app_t;

// ============================================================================
// App Lifecycle
// ============================================================================

sdk_app_t* sdk_app_create(const CHAR16 *title, INT32 width, INT32 height) {
    sdk_app_t *app = (sdk_app_t*)heap_alloc(sizeof(sdk_app_t));
    if (app == NULL) {
        return NULL;
    }
    
    // Initialize title
    if (title != NULL) {
        os_strcpy16(app->title, title, sizeof(app->title) / sizeof(CHAR16));
    } else {
        app->title[0] = L'\0';
    }
    
    // Initialize window
    app->width = width;
    app->height = height;
    app->x = 0;
    app->y = 0;
    
    // Initialize lifecycle callbacks
    app->on_init = NULL;
    app->on_update = NULL;
    app->on_cleanup = NULL;
    
    // Initialize state
    app->state = SDK_APP_STATE_INIT;
    app->running = TRUE;
    app->refresh_pending = TRUE;
    
    // Create root container
    app->root_container = sdk_container_create(0, 0, width, height);
    if (app->root_container == NULL) {
        heap_free(app);
        return NULL;
    }
    
    app->bg_color = SDK_COLOR_BG_LIGHT;
    
    // Initialize frame timing
    app->frame_count = 0;
    app->frame_time_ms = 16; // Default ~60fps
    
    // Initialize user data
    app->user_data = NULL;
    
    return app;
}

void sdk_app_destroy(sdk_app_t *app) {
    if (app == NULL) {
        return;
    }
    
    // Call cleanup callback if state is appropriate
    if (app->state != SDK_APP_STATE_SHUTDOWN && app->on_cleanup != NULL) {
        app->on_cleanup(app);
    }
    
    // Destroy container
    if (app->root_container != NULL) {
        sdk_container_destroy(app->root_container);
    }
    
    heap_free(app);
}

void sdk_app_set_lifecycle(sdk_app_t *app,
                          sdk_app_init_t on_init,
                          sdk_app_update_t on_update,
                          sdk_app_cleanup_t on_cleanup) {
    if (app == NULL) {
        return;
    }
    
    app->on_init = on_init;
    app->on_update = on_update;
    app->on_cleanup = on_cleanup;
}

// ============================================================================
// App Execution
// ============================================================================

BOOLEAN sdk_app_update(sdk_app_t *app) {
    if (app == NULL) {
        return FALSE;
    }
    
    // Handle initialization state
    if (app->state == SDK_APP_STATE_INIT) {
        if (app->on_init != NULL) {
            app->on_init(app);
        }
        app->state = SDK_APP_STATE_RUNNING;
    }
    
    // Check for shutdown request
    if (app->state == SDK_APP_STATE_SHUTDOWN) {
        app->running = FALSE;
        return FALSE;
    }
    
    // Handle input (would come from event system in real app)
    // For now, we'll just process frame
    
    // Call update callback
    if (app->state == SDK_APP_STATE_RUNNING && app->on_update != NULL) {
        app->on_update(app);
    }
    
    // Render
    if (app->refresh_pending || app->state == SDK_APP_STATE_RUNNING) {
        // Clear screen with background color
        sdk_graphics_rect(0, 0, 1024, 768, app->bg_color);
        
        // Render container and all components
        if (app->root_container != NULL) {
            sdk_container_render(app->root_container);
        }
        
        // Present to screen
        sdk_graphics_present();
        app->refresh_pending = FALSE;
    }
    
    // Update frame counter
    app->frame_count++;
    
    return app->running;
}

sdk_app_state_t sdk_app_get_state(sdk_app_t *app) {
    if (app == NULL) {
        return SDK_APP_STATE_SHUTDOWN;
    }
    
    return app->state;
}

void sdk_app_set_state(sdk_app_t *app, sdk_app_state_t state) {
    if (app == NULL) {
        return;
    }
    
    app->state = state;
}

void sdk_app_request_shutdown(sdk_app_t *app) {
    if (app == NULL) {
        return;
    }
    
    app->state = SDK_APP_STATE_SHUTDOWN;
}

BOOLEAN sdk_app_is_running(sdk_app_t *app) {
    if (app == NULL) {
        return FALSE;
    }
    
    return app->running && app->state != SDK_APP_STATE_SHUTDOWN;
}

// ============================================================================
// App Properties
// ============================================================================

const CHAR16* sdk_app_get_title(sdk_app_t *app) {
    if (app == NULL) {
        return L"";
    }
    
    return app->title;
}

void sdk_app_set_title(sdk_app_t *app, const CHAR16 *title) {
    if (app == NULL || title == NULL) {
        return;
    }
    
    os_strcpy16(app->title, title, sizeof(app->title) / sizeof(CHAR16));
}

void sdk_app_get_size(sdk_app_t *app, INT32 *width, INT32 *height) {
    if (app == NULL) {
        return;
    }
    
    if (width != NULL) *width = app->width;
    if (height != NULL) *height = app->height;
}

void sdk_app_get_position(sdk_app_t *app, INT32 *x, INT32 *y) {
    if (app == NULL) {
        return;
    }
    
    if (x != NULL) *x = app->x;
    if (y != NULL) *y = app->y;
}

// ============================================================================
// Container and Component Management
// ============================================================================

sdk_container_t* sdk_app_get_container(sdk_app_t *app) {
    if (app == NULL) {
        return NULL;
    }
    
    return app->root_container;
}

void sdk_app_set_background(sdk_app_t *app, UINT32 color) {
    if (app == NULL) {
        return;
    }
    
    app->bg_color = color;
}

// ============================================================================
// Rendering
// ============================================================================

void sdk_app_request_refresh(sdk_app_t *app) {
    if (app == NULL) {
        return;
    }
    
    app->refresh_pending = TRUE;
}

BOOLEAN sdk_app_has_pending_refresh(sdk_app_t *app) {
    if (app == NULL) {
        return FALSE;
    }
    
    return app->refresh_pending;
}

// ============================================================================
// User Data Storage
// ============================================================================

void sdk_app_set_user_data(sdk_app_t *app, void *user_data) {
    if (app == NULL) {
        return;
    }
    
    app->user_data = user_data;
}

void* sdk_app_get_user_data(sdk_app_t *app) {
    if (app == NULL) {
        return NULL;
    }
    
    return app->user_data;
}

// ============================================================================
// Frame Counter and Timing
// ============================================================================

UINT32 sdk_app_get_frame_count(sdk_app_t *app) {
    if (app == NULL) {
        return 0;
    }
    
    return app->frame_count;
}

UINT32 sdk_app_get_frame_time_ms(sdk_app_t *app) {
    if (app == NULL) {
        return 0;
    }
    
    return app->frame_time_ms;
}

// ============================================================================
// Built-in App Templates
// ============================================================================

sdk_app_t* sdk_app_template_list(const CHAR16 *title,
                                const CHAR16 **item_names,
                                UINTN item_count) {
    // Create basic app - template implementation simplified
    // Full template would require creating individual UI components
    (void)item_names;    // Suppress unused parameter warning
    (void)item_count;
    
    return sdk_app_create(title, 1024, 768);
}

sdk_app_t* sdk_app_template_form(const CHAR16 *title,
                                const CHAR16 **field_labels,
                                UINTN field_count) {
    // Create basic app - template implementation simplified
    // Full template would require creating individual UI components
    (void)field_labels;  // Suppress unused parameter warning
    (void)field_count;
    
    return sdk_app_create(title, 1024, 768);
}
