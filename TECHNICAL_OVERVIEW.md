# Mars UEFI OS - Technical Overview for SDK Design

## Executive Summary

Mars is a cooperative multitasking UEFI OS with a text-based GUI, event-driven architecture, and kernel-based application framework. Applications are managed by the kernel through manifests, cooperative task scheduling, and event-based inter-process communication. The system uses a window manager (WM) that renders to a framebuffer and routes input events to focused applications.

---

## 1. APPLICATION SYSTEM

### 1.1 Application Architecture

**Current Model:**
- **Kernel-managed applications**: Apps are not standalone binaries; they're part of the kernel as task entries
- **Cooperative execution**: Apps are scheduled tasks that yield control voluntarily
- **Event-driven**: Apps receive input and heartbeat events through the event bus
- **Content-based rendering**: Apps render by updating text content buffers, not direct framebuffer access

### 1.2 Application Manifest

```c
typedef struct {
    CHAR16 id[24];              // Unique app identifier (e.g., L"shell")
    CHAR16 title[32];           // Display title for window
    UINT32 capabilities;        // Process capabilities (CAP_INPUT, CAP_GRAPHICS, CAP_STORAGE, CAP_SYSTEM)
    INT32 start_x;              // Initial window position X
    INT32 start_y;              // Initial window position Y
    INT32 width;                // Initial window width
    INT32 height;               // Initial window height
} app_manifest_t;
```

**Core Suite Applications:**
```c
const app_manifest_t defaults[] = {
    { L"shell", L"System Shell", CAP_SYSTEM | CAP_INPUT, 90, 80, 420, 280 },
    { L"files", L"File Browser", CAP_STORAGE | CAP_GRAPHICS, 160, 120, 420, 300 },
    { L"term", L"Terminal", CAP_SYSTEM | CAP_INPUT, 240, 150, 460, 300 },
    { L"settings", L"Settings", CAP_GRAPHICS, 320, 180, 360, 260 },
    { L"tasks", L"Task Manager", CAP_SYSTEM, 400, 210, 360, 260 },
    { L"logs", L"System Logs", CAP_SYSTEM, 460, 240, 380, 160 }
};
```

### 1.3 Application Instance Structure

```c
typedef struct app_instance_t {
    app_manifest_t manifest;           // App metadata
    UINT32 pid;                        // Process ID
    UINT32 window_id;                  // Window ID for rendering
    UINT64 ticks;                      // Application heartbeat counter
    CHAR16 content[512];               // Text content buffer for rendering
    UINTN content_len;                 // Length of content
    CHAR16 history[512];               // History buffer (for console apps)
    UINTN history_len;                 // History length
    CHAR16 input_line[96];             // Current input line (for console apps)
    UINTN input_len;                   // Input length
} app_instance_t;
```

### 1.4 Application Task Entry Point

**Current Pattern - Cooperative Task:**

```c
typedef BOOLEAN (*app_task_entry_t)(void *context);

static BOOLEAN app_task_step(void *context) {
    app_instance_t *instance = (app_instance_t *)context;
    if (instance == NULL) {
        return FALSE;
    }
    
    // Process up to 12 events per step
    event_packet_t packet;
    UINTN processed = 0;
    while (processed < 12 && event_bus_receive(instance->pid, &packet)) {
        // Handle EVENT_CODE_APP_INPUT events
        if (packet.code == EVENT_CODE_APP_INPUT && 
            packet.payload_size == sizeof(input_event_t)) {
            input_event_t input;
            // Copy payload to input event
            UINT8 *dst = (UINT8 *)&input;
            for (UINTN i = 0; i < sizeof(input_event_t); ++i) {
                dst[i] = packet.payload[i];
            }
            handle_app_input(instance, &input);
        }
        ++processed;
    }
    
    ++instance->ticks;
    if ((instance->ticks % 120) == 0) {
        // Heartbeat every 120 ticks
    }
    
    // Update content for rendering
    (void)wm_set_window_content(instance->window_id, instance->content);
    
    return TRUE;  // Yield to scheduler
}
```

**Key Characteristics:**
- Returns `BOOLEAN`: `TRUE` = continue scheduling, `FALSE` = stop
- Called per scheduler cycle (typically every millisecond or less)
- Must be **non-blocking**: no long loops or I/O waits
- Context is the `app_instance_t*` pointer
- Must drain events from event queue (up to 12 per step)
- Update `instance->content` buffer to render text

### 1.5 Application Input Handling

**Input Event Structure:**
```c
typedef enum {
    INPUT_EVENT_KEY_DOWN = 0,
    INPUT_EVENT_MOUSE_MOVE,
    INPUT_EVENT_MOUSE_BUTTON_DOWN,
    INPUT_EVENT_MOUSE_BUTTON_UP
} input_event_type_t;

typedef struct {
    input_event_type_t type;
    union {
        struct {
            CHAR16 unicode;          // Key character (e.g., L'a')
            UINT16 scan_code;        // UEFI scan code
        } key;
        struct {
            INT32 dx;                // Relative movement
            INT32 dy;
            INT32 x;                 // Absolute position
            INT32 y;
        } mouse_move;
        struct {
            BOOLEAN left;            // Left button state
            BOOLEAN right;           // Right button state
        } mouse_button;
    } data;
} input_event_t;
```

**Event Delivery Flow:**
1. Input driver (input_poll) publishes events to EVENT_CHANNEL_INPUT
2. WM service (wm_dispatch_input) consumes and routes to focused window
3. WM creates EVENT_CODE_APP_INPUT event with input event as payload
4. App receives via event_bus_receive(pid, &packet)
5. App deserializes input_event_t from packet.payload

### 1.6 Application Rendering (Content-Based)

**Current Rendering Model:**

Apps do **not** render directly to framebuffer. Instead:
1. App maintains text content in `instance->content` buffer (512 chars max)
2. App calls `wm_set_window_content(window_id, content)`
3. Window manager renders the text content to window

**Content Format:**
- UTF-16 text with `L'\n'` line breaks
- Max 512 characters total
- Each line renders as 8-pixel-wide characters at 16-pixel height
- Automatic line wrapping based on window width

### 1.7 App Framework APIs

**Registration:**
```c
BOOLEAN app_register(const app_manifest_t *manifest);
```

**Launching:**
```c
BOOLEAN app_launch(const CHAR16 *id);  // Launch by ID (must be registered first)
void app_launch_core_suite(void);       // Launch all default apps
```

**Initialization:**
```c
void app_framework_init(void);  // Called once at kernel startup
```

---

## 2. EVENT SYSTEM

### 2.1 Event Bus Architecture

**Design:**
- Publish-subscribe with **two independent queue systems**
- Channel-based events (broadcast to all subscribers)
- Process-targeted events (unicast to specific PID)
- Bounded, non-blocking queues with backpressure policies

**Event Channels:**
```c
#define EVENT_CHANNEL_INPUT 1              // Mouse/pointer events
#define EVENT_CHANNEL_INPUT_KEYBOARD 2     // Keyboard events
#define EVENT_CHANNEL_SYSTEM 3             // System/IPC events
#define EVENT_CHANNEL_APP 4                // App-specific events
#define EVENT_CHANNEL_COUNT 5
```

### 2.2 Event Packet Structure

```c
typedef struct {
    UINT32 channel;           // Channel ID (EVENT_CHANNEL_*)
    UINT32 code;              // Event code (EVENT_CODE_*)
    UINT32 source_pid;        // Publishing process
    UINT32 target_pid;        // If non-zero, also queue to process
    UINT32 target_window;     // Associated window ID
    UINT32 payload_size;      // Size of payload (max 64 bytes)
    UINT8 payload[64];        // Event-specific data
} event_packet_t;
```

**Event Codes:**
```c
#define EVENT_CODE_INPUT 1                 // Input event (mouse/keyboard)
#define EVENT_CODE_TIMER_TICK 2            // Timer tick
#define EVENT_CODE_APP_HEARTBEAT 3         // App periodic heartbeat
#define EVENT_CODE_APP_INPUT 4             // App input (routed from WM)
#define EVENT_CODE_APP_LAUNCH_REQUEST 5    // Launch application request
```

### 2.3 Event Bus APIs

```c
void event_bus_init(void);

BOOLEAN event_bus_publish(const event_packet_t *packet);

BOOLEAN event_bus_receive(UINT32 pid, event_packet_t *out_packet);

BOOLEAN event_bus_receive_channel(UINT32 channel, event_packet_t *out_packet);

BOOLEAN event_bus_register_process(UINT32 pid);

void event_bus_unregister_process(UINT32 pid);

BOOLEAN event_bus_set_channel_policy(UINT32 channel, 
                                     event_backpressure_policy_t policy);

UINTN event_bus_channel_depth(UINT32 channel);

UINTN event_bus_process_depth(UINT32 pid);

UINT64 event_bus_channel_drop_count(void);

UINT64 event_bus_process_drop_count(void);
```

### 2.4 Backpressure Policies

**When queue is full:**
```c
typedef enum {
    EVENT_BACKPRESSURE_DROP_NEWEST = 0,  // Reject new event
    EVENT_BACKPRESSURE_DROP_OLDEST = 1   // Remove oldest, accept new
} event_backpressure_policy_t;
```

---

## 3. UI/RENDERING SYSTEM

### 3.1 Window Manager Architecture

The WM is a **service task** that runs every scheduler cycle.

**WM Service Components:**
```c
static BOOLEAN wm_task(void *context) {
    (void)context;
    wm_dispatch_input();  // Process input events
    return TRUE;
}

static BOOLEAN render_task(void *context) {
    (void)context;
    if (wm_needs_redraw()) {
        wm_render();      // Render to framebuffer
    }
    return TRUE;
}
```

### 3.2 Window Structure

```c
typedef struct {
    UINT32 id;           // Unique window ID
    UINT32 owner_pid;    // Process that owns the window
    INT32 x, y;          // Position on desktop
    INT32 width, height; // Dimensions
    UINT8 z;             // Z-order (stacking)
    BOOLEAN focused;     // Has input focus
    BOOLEAN visible;     // Is visible
    BOOLEAN invalidated; // Needs redraw
    CHAR16 title[32];    // Window title
} wm_window_t;
```

### 3.3 Window Management APIs

```c
void wm_init(UINT32 desktop_w, UINT32 desktop_h);

UINT32 wm_create_window(UINT32 owner_pid, const CHAR16 *title,
                       INT32 x, INT32 y, INT32 width, INT32 height);

BOOLEAN wm_set_window_content(UINT32 window_id, const CHAR16 *text);

BOOLEAN wm_focus_window(UINT32 window_id);

UINT32 wm_focused_window(void);

void wm_set_debug_overlay(BOOLEAN enabled);

BOOLEAN wm_debug_overlay_enabled(void);
```

### 3.4 UI Components (Text-Based)

**Windows:**
- 24-pixel tall title bar with window title
- Minimize button (top-right, yellow)
- Close button (top-right-right, red)
- Content area with text rendering
- 8x8 resize handle (bottom-right)

**Taskbar:**
- 30 pixels tall at bottom
- "Start" button (72x22 pixels)
- "Mars Desktop" label
- Clock display (HH:MM:SS format)

**Start Menu:**
- Dropdown from Start button
- 6 items: Shell, Files, Terminal, Settings, Tasks, Logs
- Item height: 26 pixels

**Text Rendering:**
- 8x16 pixel monospace glyphs
- Max ~50 chars per line
- Auto line-wrapping

### 3.5 Rendering Primitives

```c
void drawPixel(INT32 x, INT32 y, UINT32 color);
void drawRect(INT32 x, INT32 y, INT32 width, INT32 height, UINT32 color);
void drawChar(INT32 x, INT32 y, CHAR16 c, UINT32 fg, UINT32 bg);
void drawString(INT32 x, INT32 y, const CHAR16 *text, UINT32 fg, UINT32 bg);
void clearScreen(UINT32 color);
void framebuffer_present(void);
void framebuffer_present_region(INT32 x, INT32 y, INT32 width, INT32 height);
```

---

## 4. PROCESS & SCHEDULER

### 4.1 Process Model

```c
typedef enum {
    PROCESS_NEW = 0,
    PROCESS_RUNNING,
    PROCESS_WAITING,
    PROCESS_TERMINATED
} process_state_t;

typedef enum {
    CAP_INPUT = 1 << 0,     // Input access
    CAP_GRAPHICS = 1 << 1,  // Framebuffer access
    CAP_STORAGE = 1 << 2,   // Storage/VFS access
    CAP_SYSTEM = 1 << 3     // System/privileged operations
} process_capability_t;
```

### 4.2 Process Creation

```c
UINT32 process_create_kernel(const CHAR16 *name, task_entry_t entry, 
                             void *context, UINT8 priority, 
                             UINT32 capabilities, BOOLEAN is_kernel);
```

### 4.3 Scheduler APIs

```c
void scheduler_init(void);

UINT32 scheduler_create_task(UINT32 owner_pid, const CHAR16 *name,
                            UINT8 priority, task_entry_t entry, 
                            void *context);

void scheduler_run(void);  // Main loop - never returns

UINTN scheduler_task_count(void);

BOOLEAN scheduler_task_at(UINTN index, task_t *out_task);
```

### 4.4 Scheduler Model

**Cooperative Round-Robin:**
- All tasks scheduled sequentially
- Each task must return (yield) within a cycle
- No preemption unless timer-preemptive mode enabled

---

## 5. MEMORY & VM

### 5.1 Memory Management

**Heap (for small allocations):**
```c
void heap_init(UINTN initial_pages);
void *heap_alloc(UINTN size);
void heap_free(void *ptr);
```

**Page Allocator (for large allocations):**
```c
EFI_PHYSICAL_ADDRESS memory_alloc_pages(UINTN page_count);
BOOLEAN memory_release_pages(EFI_PHYSICAL_ADDRESS address, UINTN page_count);
```

### 5.2 Framebuffer Access

```c
typedef struct {
    UINT32 *base;      // Pixel array (ARGB)
    UINT32 width;
    UINT32 height;
    UINT32 pitch;      // Bytes per scanline
} framebuffer_t;

void framebuffer_init(const platform_context_t *platform);
```

---

## 6. EXAMPLE APP PATTERNS

### 6.1 Console Application Pattern (shell, term)

```c
// App task entry
static BOOLEAN console_task(void *context) {
    app_instance_t *instance = (app_instance_t *)context;
    
    // Process events
    event_packet_t packet;
    UINTN processed = 0;
    while (processed < 12 && event_bus_receive(instance->pid, &packet)) {
        if (packet.code == EVENT_CODE_APP_INPUT) {
            input_event_t input;
            // Deserialize from payload
            handle_app_input(instance, &input);
        }
        ++processed;
    }
    
    // Rebuild view
    compose_console_view(instance);
    
    // Update window
    wm_set_window_content(instance->window_id, instance->content);
    
    return TRUE;
}

// Content composition
static void compose_console_view(app_instance_t *instance) {
    instance->content[0] = 0;
    instance->content_len = 0;
    
    // Append history + prompt + input
    buffer_append_text(instance->content, 512, 
                      &instance->content_len, instance->history);
    if (instance->content_len > 0) {
        buffer_append_char(instance->content, 512, 
                         &instance->content_len, L'\n');
    }
    buffer_append_text(instance->content, 512, 
                      &instance->content_len, L"shell> ");
    buffer_append_text(instance->content, 512, 
                      &instance->content_len, instance->input_line);
}
```

### 6.2 Dynamic Content Application Pattern (tasks, logs)

```c
// App task entry
static BOOLEAN tasks_task(void *context) {
    app_instance_t *instance = (app_instance_t *)context;
    
    // Process events...
    
    // Every 10 ticks, refresh content from system state
    ++instance->ticks;
    if ((instance->ticks % 10) == 0) {
        render_tasks_content(instance);
    }
    
    wm_set_window_content(instance->window_id, instance->content);
    return TRUE;
}

// Content rendering from system queries
static void render_tasks_content(app_instance_t *instance) {
    instance->content[0] = 0;
    instance->content_len = 0;
    
    // Query scheduler and build list
    UINTN total = scheduler_task_count();
    for (UINTN i = 0; i < total; ++i) {
        task_t task;
        if (!scheduler_task_at(i, &task)) continue;
        
        // Format and append task info...
    }
}
```

---

## 7. KEY CONSTRAINTS

1. **No Direct Framebuffer Access**: Apps render via text content buffers only
2. **Cooperative Execution**: Apps must return quickly (no long loops/blocking)
3. **Event-Driven**: All communication through event bus
4. **Single Address Space**: No memory protection between processes
5. **Bounded Resources**: Fixed 16 windows, max 128 events per queue
6. **Kernel-Integrated**: Apps are kernel tasks, not separate binaries

---

## 8. CURRENT CAPABILITIES SUMMARY

| Component | Capability | Limit |
|-----------|-----------|-------|
| Windows | Text content rendering | Max 16 |
| Input | Keyboard + Mouse routing | Focused window only |
| Event Channels | Broadcast queues | 128 events each |
| Process Queues | Targeted delivery | 128 events each |
| Text Buffer | UTF-16 content | 512 chars/window |
| Text Glyphs | 8x16 monospace | From font8x16.h |
| Events/Step | Per consumer limit | Input: 16, App: 12 |
| Heartbeat | App tick counter | Every 120 ticks |
| Memory | Heap + page allocation | Cooperative bounds |

---

## 9. SDK DESIGN OPPORTUNITIES

The SDK should:
- **Enhance** existing patterns (don't replace them)
- **Simplify** event serialization/deserialization
- **Provide** utility functions for common operations
- **Support** text-based UI components (buttons, input, lists)
- **Enable** custom event codes within EVENT_CHANNEL_APP
- **Allow** inter-app communication patterns
- **Facilitate** app logic testing
- **Document** best practices for memory, timing, events

---

**End of Technical Overview**
