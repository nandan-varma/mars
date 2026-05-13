/**
 * @file wm_internal.h
 * @brief Internal window manager definitions and functions
 */

#ifndef WM_INTERNAL_H
#define WM_INTERNAL_H

#include "wm.h"

#include "event_bus.h"
#include "input.h"

#define WM_MAX_WINDOWS 16
#define START_BUTTON_W 72
#define START_BUTTON_H 22
#define START_MENU_W 238
#define START_MENU_ITEM_H 26
#define START_MENU_ITEM_COUNT 6
#define WM_POINTER_EVENTS_PER_STEP 16
#define WM_KEY_EVENTS_PER_STEP 8
#define TASKBAR_H 30
#define WM_CONTENT_CHARS 512

// COMPILE-TIME SAFETY: Ensure WM_MAX_WINDOWS is reasonable
_Static_assert(WM_MAX_WINDOWS <= 256, "WM_MAX_WINDOWS exceeds reasonable limit");

/**
 * @brief Structure representing a start menu item
 */
typedef struct {
    const CHAR16 *id;     /**< Application ID */
    const CHAR16 *label;  /**< Display label */
} start_item_t;

/**
 * @brief Global window manager state structure
 */
typedef struct {
    wm_window_t windows[WM_MAX_WINDOWS];  /**< Array of windows */
    UINTN window_count;                   /**< Current number of windows */
    UINT32 next_window_id;                /**< Next available window ID */
    UINT32 focused_window;                /**< Currently focused window ID */
    UINT32 desktop_w;                     /**< Desktop width */
    UINT32 desktop_h;                     /**< Desktop height */
    BOOLEAN dirty;                        /**< Flag indicating if redraw is needed */
    BOOLEAN dragging;                     /**< Flag indicating if a window is being dragged */
    BOOLEAN resizing;                     /**< Flag indicating if a window is being resized */
    UINT32 active_window;                 /**< Currently active window ID */
    INT32 drag_offset_x;                  /**< Drag offset X coordinate */
    INT32 drag_offset_y;                  /**< Drag offset Y coordinate */
    CHAR16 content[WM_MAX_WINDOWS][WM_CONTENT_CHARS];  /**< Window content buffers */
    UINT8 next_z;                         /**< Next Z-order value */
    BOOLEAN start_menu_open;              /**< Flag indicating if start menu is open */
    UINT64 fps_last_tick;                 /**< Last FPS measurement tick */
    UINT32 fps_counter;                   /**< FPS counter */
    UINT32 fps_value;                     /**< Current FPS value */
    BOOLEAN show_debug_overlay;           /**< Flag to show debug overlay */
    UINT64 clock_last_second;             /**< Last clock second update */
    BOOLEAN clock_only_redraw;            /**< Flag for clock-only redraw */
    UINT64 fallback_clock_second;         /**< Fallback clock second */
    UINTN fallback_clock_hz;              /**< Fallback clock frequency */
} wm_state_t;

/**
 * @brief Get the global window manager state
 * @return Pointer to the global wm_state_t structure
 */
wm_state_t *wm_state(void);

/**
 * @brief Get the start menu items
 * @param count Output parameter for the number of items
 * @return Pointer to array of start_item_t structures
 */
const start_item_t *wm_start_items(UINTN *count);

/**
 * @brief Publish a launch request for an application
 * @param app_id The application ID to launch
 */
void wm_publish_launch_request(const CHAR16 *app_id);

/**
 * @brief Find a window by its ID
 * @param id The window ID to search for
 * @return Pointer to the wm_window_t structure, or NULL if not found
 */
wm_window_t *wm_find_window(UINT32 id);

/**
 * @brief Get the index of a window by its ID
 * @param id The window ID
 * @return Index of the window, or WM_MAX_WINDOWS if not found
 */
UINTN wm_window_index_by_id(UINT32 id);

/**
 * @brief Focus a window internally (without publishing events)
 * @param id The window ID to focus
 */
void wm_focus_window_internal(UINT32 id);

/**
 * @brief Find the topmost window at the given mouse coordinates
 * @param mouse_x Mouse X coordinate
 * @param mouse_y Mouse Y coordinate
 * @return Pointer to the wm_window_t structure, or NULL if none found
 */
wm_window_t *wm_top_window_at(INT32 mouse_x, INT32 mouse_y);

/**
 * @brief Handle a click on the start menu
 * @param mouse_x Mouse X coordinate
 * @param mouse_y Mouse Y coordinate
 * @return TRUE if the click was handled, FALSE otherwise
 */
BOOLEAN wm_handle_start_menu_click(INT32 mouse_x, INT32 mouse_y);

/**
 * @brief Render the start menu
 */
void wm_render_start_menu(void);

/**
 * @brief Get the current second of the day
 * @return Current second of the day
 */
UINT64 wm_current_second_of_day(void);

/**
 * @brief Format the current time into a string
 * @param out Output buffer for the formatted time
 * @param max_chars Maximum number of characters in the output buffer
 */
void wm_format_clock_text(CHAR16 *out, UINTN max_chars);

#endif
