#include "sdk/sdk_core.h"
#include "sdk/sdk_layout.h"
#include "sdk/sdk_ui.h"
#include "sdk/sdk_graphics.h"
#include "heap.h"

// ============================================================================
// Internal Container Structure
// ============================================================================

typedef struct {
    INT32 x, y;
    INT32 width, height;
    BOOLEAN manual_position;
} sdk_component_layout_t;

typedef struct sdk_container {
    // Position and size
    INT32 x, y;
    INT32 width, height;
    
    // Layout configuration
    sdk_layout_type_t layout_type;
    INT32 spacing;
    INT32 padding;
    UINTN grid_cols;
    
    // Components
    sdk_component_t *components[32];
    sdk_component_layout_t component_layouts[32];
    UINTN component_count;
    
    // Appearance
    UINT32 bg_color;
    INT32 border_thickness;
    UINT32 border_color;
    
    // State
    BOOLEAN visible;
    BOOLEAN enabled;
} sdk_container_t;

// ============================================================================
// Container Lifecycle
// ============================================================================

sdk_container_t* sdk_container_create(INT32 x, INT32 y, INT32 width, INT32 height) {
    sdk_container_t *container = (sdk_container_t*)heap_alloc(sizeof(sdk_container_t));
    if (container == NULL) {
        return NULL;
    }
    
    container->x = x;
    container->y = y;
    container->width = width;
    container->height = height;
    
    container->layout_type = SDK_LAYOUT_VERTICAL;
    container->spacing = 10;
    container->padding = 5;
    container->grid_cols = 1;
    
    container->component_count = 0;
    
    container->bg_color = SDK_COLOR_BG_LIGHT;
    container->border_thickness = 0;
    container->border_color = SDK_COLOR_BORDER;
    
    container->visible = TRUE;
    container->enabled = TRUE;
    
    return container;
}

void sdk_container_destroy(sdk_container_t *container) {
    if (container != NULL) {
        heap_free(container);
    }
}

// ============================================================================
// Layout Configuration
// ============================================================================

void sdk_container_set_layout(sdk_container_t *container, 
                              sdk_layout_type_t layout_type,
                              INT32 spacing,
                              INT32 padding) {
    if (container == NULL) {
        return;
    }
    
    container->layout_type = layout_type;
    container->spacing = spacing;
    container->padding = padding;
    sdk_container_layout_update(container);
}

void sdk_container_set_grid_cols(sdk_container_t *container, UINTN cols) {
    if (container == NULL || cols < 1) {
        return;
    }
    
    container->grid_cols = cols;
    sdk_container_layout_update(container);
}

// ============================================================================
// Component Management
// ============================================================================

BOOLEAN sdk_container_add_component(sdk_container_t *container, 
                                    sdk_component_t *component) {
    if (container == NULL || component == NULL) {
        return FALSE;
    }
    
    if (container->component_count >= 32) {
        return FALSE;
    }
    
    container->components[container->component_count] = component;
    container->component_layouts[container->component_count].manual_position = FALSE;
    container->component_count++;
    
    sdk_container_layout_update(container);
    return TRUE;
}

void sdk_container_remove_component(sdk_container_t *container,
                                    sdk_component_t *component) {
    if (container == NULL || component == NULL) {
        return;
    }
    
    for (UINTN i = 0; i < container->component_count; i++) {
        if (container->components[i] == component) {
            // Shift remaining components
            for (UINTN j = i; j < container->component_count - 1; j++) {
                container->components[j] = container->components[j + 1];
                container->component_layouts[j] = container->component_layouts[j + 1];
            }
            container->component_count--;
            break;
        }
    }
}

void sdk_container_clear(sdk_container_t *container) {
    if (container == NULL) {
        return;
    }
    
    container->component_count = 0;
}

UINTN sdk_container_get_component_count(sdk_container_t *container) {
    if (container == NULL) {
        return 0;
    }
    
    return container->component_count;
}

// ============================================================================
// Container Properties
// ============================================================================

void sdk_container_set_background(sdk_container_t *container, UINT32 color) {
    if (container == NULL) {
        return;
    }
    
    container->bg_color = color;
}

void sdk_container_set_border(sdk_container_t *container, INT32 thickness, UINT32 color) {
    if (container == NULL) {
        return;
    }
    
    container->border_thickness = thickness;
    container->border_color = color;
}

void sdk_container_set_visible(sdk_container_t *container, BOOLEAN visible) {
    if (container == NULL) {
        return;
    }
    
    container->visible = visible;
}

void sdk_container_set_enabled(sdk_container_t *container, BOOLEAN enabled) {
    if (container == NULL) {
        return;
    }
    
    container->enabled = enabled;
}

void sdk_container_get_bounds(sdk_container_t *container,
                             INT32 *x, INT32 *y, INT32 *width, INT32 *height) {
    if (container == NULL) {
        return;
    }
    
    if (x != NULL) *x = container->x;
    if (y != NULL) *y = container->y;
    if (width != NULL) *width = container->width;
    if (height != NULL) *height = container->height;
}

void sdk_container_set_bounds(sdk_container_t *container,
                             INT32 x, INT32 y, INT32 width, INT32 height) {
    if (container == NULL) {
        return;
    }
    
    container->x = x;
    container->y = y;
    container->width = width;
    container->height = height;
    sdk_container_layout_update(container);
}

// ============================================================================
// Layout Engine Implementation
// ============================================================================

static void sdk_container_layout_vertical(sdk_container_t *container) {
    INT32 current_y = container->y + container->padding;
    INT32 component_width = container->width - 2 * container->padding;
    
    for (UINTN i = 0; i < container->component_count; i++) {
        if (container->component_layouts[i].manual_position) {
            continue;
        }
        
         // Set component position and size
         sdk_component_t *comp = container->components[i];
         if (comp == NULL) {
             continue;
         }
         
         sdk_component_set_position(comp, 
                                   container->x + container->padding,
                                   current_y);
         sdk_component_set_size(comp, component_width, 32);
         
         current_y += 32 + container->spacing;
    }
}

static void sdk_container_layout_horizontal(sdk_container_t *container) {
    INT32 current_x = container->x + container->padding;
    INT32 component_height = container->height - 2 * container->padding;
    
    for (UINTN i = 0; i < container->component_count; i++) {
        if (container->component_layouts[i].manual_position) {
            continue;
        }
        
        sdk_component_t *comp = container->components[i];
        if (comp == NULL) {
            continue;
        }
        
        // Distribute width evenly
         INT32 available_width = container->width - 2 * container->padding;
         INT32 total_spacing = container->spacing * (container->component_count - 1);
         INT32 component_width = (available_width - total_spacing) / container->component_count;
         
         sdk_component_set_position(comp,
                                   current_x,
                                   container->y + container->padding);
         sdk_component_set_size(comp, component_width, component_height);
         
         current_x += component_width + container->spacing;
    }
}

static void sdk_container_layout_grid(sdk_container_t *container) {
    INT32 current_x = container->x + container->padding;
    INT32 current_y = container->y + container->padding;
    INT32 col = 0;
    
    INT32 available_width = container->width - 2 * container->padding;
    INT32 cell_width = available_width / container->grid_cols;
    INT32 cell_height = 32; // Default height
    
    for (UINTN i = 0; i < container->component_count; i++) {
        if (container->component_layouts[i].manual_position) {
            continue;
        }
        
        sdk_component_t *comp = container->components[i];
        if (comp == NULL) {
            continue;
         }
         
         sdk_component_set_position(comp,
                                   current_x,
                                   current_y);
         sdk_component_set_size(comp, cell_width - container->spacing, cell_height);
         
         col++;
         if (col >= (INT32)container->grid_cols) {
             col = 0;
             current_x = container->x + container->padding;
             current_y += cell_height + container->spacing;
         } else {
             current_x += cell_width;
        }
    }
}

void sdk_container_layout_update(sdk_container_t *container) {
    if (container == NULL) {
        return;
    }
    
    switch (container->layout_type) {
        case SDK_LAYOUT_VERTICAL:
            sdk_container_layout_vertical(container);
            break;
        case SDK_LAYOUT_HORIZONTAL:
            sdk_container_layout_horizontal(container);
            break;
        case SDK_LAYOUT_GRID:
            sdk_container_layout_grid(container);
            break;
        default:
            break;
    }
}

// ============================================================================
// Rendering and Input
// ============================================================================

void sdk_container_render(sdk_container_t *container) {
    if (container == NULL || !container->visible) {
        return;
    }
    
    // Draw background
    sdk_graphics_rect(container->x, container->y, container->width, container->height, 
                     container->bg_color);
    
    // Draw border
    if (container->border_thickness > 0) {
        sdk_graphics_rect_outline(container->x, container->y, container->width, container->height,
                                 container->border_color, container->border_thickness);
    }
    
    // Render all components
    for (UINTN i = 0; i < container->component_count; i++) {
        if (container->components[i] != NULL) {
            sdk_component_render(container->components[i]);
        }
    }
}

BOOLEAN sdk_container_handle_input(sdk_container_t *container,
                                   const input_event_t *event) {
    if (container == NULL || !container->enabled || event == NULL) {
        return FALSE;
    }
    
    // Try to handle input for each component
    for (UINTN i = 0; i < container->component_count; i++) {
        if (container->components[i] != NULL) {
            if (sdk_component_handle_input(container->components[i], event)) {
                return TRUE;
            }
        }
    }
    
    return FALSE;
}

// ============================================================================
// Advanced: Manual Position Override
// ============================================================================

void sdk_container_set_component_position(sdk_container_t *container,
                                         sdk_component_t *component,
                                         INT32 x, INT32 y, INT32 width, INT32 height) {
    if (container == NULL || component == NULL) {
        return;
    }
    
    for (UINTN i = 0; i < container->component_count; i++) {
        if (container->components[i] == component) {
            sdk_component_set_position(component, x, y);
            sdk_component_set_size(component, width, height);
            container->component_layouts[i].manual_position = TRUE;
            container->component_layouts[i].x = x;
            container->component_layouts[i].y = y;
            container->component_layouts[i].width = width;
            container->component_layouts[i].height = height;
            return;
        }
    }
}
