#pragma once

#include "sdk_ui.h"
#include "uefi.h"

// ============================================================================
// Layout System - Layer 5
// ============================================================================
// Provides container-based component composition with automatic layout engines.
// Supports vertical, horizontal, and grid layouts with spacing and padding.
//
// Typical usage:
//   sdk_container_t *container = sdk_container_create(100, 100, 400, 300);
//   sdk_container_set_layout(container, SDK_LAYOUT_VERTICAL, 10, 5);
//   sdk_container_add_component(container, button);
//   sdk_container_add_component(container, slider);
//   sdk_container_render(container);
//   sdk_container_handle_input(container, event);

// ============================================================================
// Type Definitions
// ============================================================================

typedef enum {
    SDK_LAYOUT_VERTICAL,     // Components stacked vertically (top to bottom)
    SDK_LAYOUT_HORIZONTAL,   // Components arranged horizontally (left to right)
    SDK_LAYOUT_GRID,         // Components in grid (cols specified)
} sdk_layout_type_t;

// Opaque container handle
typedef struct sdk_container sdk_container_t;

// ============================================================================
// Container Lifecycle
// ============================================================================

// Create a new container at position (x, y) with size (width, height)
// Returns NULL on allocation failure
sdk_container_t* sdk_container_create(INT32 x, INT32 y, INT32 width, INT32 height);

// Destroy container and free all resources
// Does NOT destroy contained components (caller responsibility)
void sdk_container_destroy(sdk_container_t *container);

// ============================================================================
// Layout Configuration
// ============================================================================

// Set the layout engine for this container
// layout_type: SDK_LAYOUT_VERTICAL, SDK_LAYOUT_HORIZONTAL, SDK_LAYOUT_GRID
// spacing: pixels between components
// padding: pixels of padding inside container
void sdk_container_set_layout(sdk_container_t *container, 
                              sdk_layout_type_t layout_type,
                              INT32 spacing,
                              INT32 padding);

// For grid layouts, set the number of columns
// Width of each cell is calculated as (container_width - 2*padding) / cols
void sdk_container_set_grid_cols(sdk_container_t *container, UINTN cols);

// ============================================================================
// Component Management
// ============================================================================

// Add a component to the container
// Container automatically positions component based on layout engine
// Returns TRUE on success, FALSE if container is full (max 32 components)
BOOLEAN sdk_container_add_component(sdk_container_t *container, 
                                    sdk_component_t *component);

// Remove a component from the container (doesn't destroy it)
void sdk_container_remove_component(sdk_container_t *container,
                                    sdk_component_t *component);

// Clear all components from container
void sdk_container_clear(sdk_container_t *container);

// Get number of components currently in container
UINTN sdk_container_get_component_count(sdk_container_t *container);

// ============================================================================
// Container Properties
// ============================================================================

// Set background color for container
void sdk_container_set_background(sdk_container_t *container, UINT32 color);

// Set border (thickness > 0 to show border)
void sdk_container_set_border(sdk_container_t *container, INT32 thickness, UINT32 color);

// Set container visibility (hidden containers don't render or handle input)
void sdk_container_set_visible(sdk_container_t *container, BOOLEAN visible);

// Set container enable state (disabled containers don't handle input)
void sdk_container_set_enabled(sdk_container_t *container, BOOLEAN enabled);

// Get container position and size
void sdk_container_get_bounds(sdk_container_t *container,
                             INT32 *x, INT32 *y, INT32 *width, INT32 *height);

// Set new position and size
void sdk_container_set_bounds(sdk_container_t *container,
                             INT32 x, INT32 y, INT32 width, INT32 height);

// ============================================================================
// Rendering and Input
// ============================================================================

// Render container and all contained components
void sdk_container_render(sdk_container_t *container);

// Handle input event for container and all contained components
// Returns TRUE if any component handled the event
BOOLEAN sdk_container_handle_input(sdk_container_t *container,
                                   const input_event_t *event);

// ============================================================================
// Layout Recalculation
// ============================================================================

// Force recalculation of component positions based on current layout
// Called automatically when components are added/removed or container is resized
void sdk_container_layout_update(sdk_container_t *container);

// ============================================================================
// Advanced: Direct Component Position Override
// ============================================================================

// Set manual position for a component (overrides layout engine)
// If manual positioning is used, layout engine won't update this component
void sdk_container_set_component_position(sdk_container_t *container,
                                         sdk_component_t *component,
                                         INT32 x, INT32 y, INT32 width, INT32 height);
