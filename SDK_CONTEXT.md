# Mars SDK Context & Design Documentation

This directory contains comprehensive technical analysis and design recommendations for building an SDK for the Mars UEFI OS.

## Documents

### 1. TECHNICAL_OVERVIEW.md (17 KB)
**Complete technical deep-dive** of the Mars OS architecture:
- Application system architecture and lifecycle
- Event bus design and implementation
- Window manager and rendering pipeline
- Process scheduling model
- Memory and VM systems
- Example application patterns
- Initialization sequence
- Security model

**Use this for:** Understanding current system design, API details, code patterns, and constraints.

### 2. This Document (SDK_CONTEXT.md)
**Quick reference** for SDK design decisions and opportunities.

## Quick Reference

### Current System Architecture

```
┌─────────────────────────────────────────────────────────────┐
│ Mars Kernel                                                 │
├─────────────────────────────────────────────────────────────┤
│ ┌──────────────────────────────────────────────────────┐   │
│ │ Managed Services (Always Running)                    │   │
│ ├──────────────────────────────────────────────────────┤   │
│ │ input-service → wm-service → render-service         │   │
│ │ supervisor-service, app-manager                      │   │
│ └──────────────────────────────────────────────────────┘   │
│                          ↓                                   │
│ ┌──────────────────────────────────────────────────────┐   │
│ │ Event Bus (Channel + Process Queues)                 │   │
│ ├──────────────────────────────────────────────────────┤   │
│ │ INPUT (128) | KEYBOARD (128) | SYSTEM (128)         │   │
│ │ APP (128) + per-process queues (128 each)           │   │
│ └──────────────────────────────────────────────────────┘   │
│                          ↓                                   │
│ ┌──────────────────────────────────────────────────────┐   │
│ │ Application Tasks (up to 16 windows)                 │   │
│ ├──────────────────────────────────────────────────────┤   │
│ │ shell, files, term, settings, tasks, logs + custom  │   │
│ │ Each: cooperative task → text content → WM render   │   │
│ └──────────────────────────────────────────────────────┘   │
│                          ↓                                   │
│ ┌──────────────────────────────────────────────────────┐   │
│ │ Framebuffer (Pixel-based Rendering)                 │   │
│ ├──────────────────────────────────────────────────────┤   │
│ │ 8x16 text glyphs, windows, taskbar, cursor          │   │
│ └──────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

### Application Lifecycle

```
1. REGISTRATION
   app_manifest_t manifest = { L"myapp", L"My App", CAP_INPUT, ... };
   app_register(&manifest);

2. LAUNCH
   app_launch(L"myapp");
   ↓
   app_instance_launch()
   ├─ heap_alloc(sizeof(app_instance_t))
   ├─ process_create_kernel(entry_fn, instance, ...)
   ├─ wm_create_window(pid, title, x, y, w, h)
   └─ content_init(instance)

3. EXECUTION (Every scheduler cycle ~1ms)
   app_task_step(instance) {
     ├─ Drain events (max 12)
     │  └─ event_bus_receive(pid, &packet)
     ├─ Process input
     ├─ Update state/content
     ├─ Render text
     │  └─ wm_set_window_content(window_id, text)
     └─ return TRUE
   }

4. TERMINATION
   Close button or process_exit(pid, code)
   ├─ wm_destroy_window()
   ├─ process_exit()
   └─ heap_free()
```

### Event Flow

```
INPUT DEVICE
    ↓
input_service (input_poll)
    ├─ Reads mouse/keyboard
    └─ event_bus_publish() → EVENT_CHANNEL_INPUT
                                    ↓
                         wm_service (wm_dispatch_input)
                              ├─ event_bus_receive_channel(INPUT)
                              └─ Deserialize input_event_t
                                    ├─ Validate event.type
                                    ├─ Route to focused window
                                    └─ event_bus_publish(target_pid=app_pid)
                                                    ↓
                                         APP PROCESS (app_task_step)
                                              └─ event_bus_receive(pid)
                                                  └─ Deserialize payload
                                                      └─ handle_input()
```

### Memory Model

```
Heap Allocation:
  - heap_alloc(size): Dynamic allocation
  - heap_free(ptr): Manual deallocation
  - Max size limited by heap pages

Page Allocation:
  - memory_alloc_pages(count): Large allocations
  - memory_release_pages(addr, count): Deallocation
  - Bounded by available system memory

String Buffers:
  - ALWAYS use max_chars parameter
  - No dynamic resizing
  - Example: CHAR16 buf[512]; buffer_append(buf, 512, &len, text);
```

### Content-Based Rendering

```
App produces text:
┌─────────────────────┐
│ shell> help         │
│ Commands:           │
│   help - Show help  │
│ shell> _            │
└─────────────────────┘

↓ wm_set_window_content(window_id, text)

WM renders with:
  ┌─────────────────────────────┐
  │ System Shell       [_] [×]  │ ← Title bar + buttons
  ├─────────────────────────────┤
  │ shell> help                 │
  │ Commands:                   │ ← Content (8x16 glyphs)
  │   help - Show help          │
  │ shell> _                    │
  │                             │
  │                             │
  │                      ↗      │ ← Resize handle
  └─────────────────────────────┘

Rules:
  - Max 512 UTF-16 chars
  - L'\n' = line break
  - Auto line-wrap at window width
  - 8x16 pixel glyphs
```

## SDK Design Principles

### What to Enhance (NOT Replace)

1. **Event Serialization**
   - Current: Manual byte copying to/from payload[64]
   - SDK should: Provide marshaling macros/functions
   - Keep: Raw event bus intact

2. **Buffer Management**
   - Current: Bounded string operations (max_chars)
   - SDK should: Provide safe append/trim utilities
   - Keep: Memory bounds discipline

3. **Text Rendering**
   - Current: Manual text composition
   - SDK should: Provide UI component builders
   - Keep: Content-based rendering model

4. **Input Processing**
   - Current: Manual event type checking
   - SDK should: Abstract common patterns (nav keys, buttons, etc.)
   - Keep: Low-level input event delivery

5. **Application Framework**
   - Current: Manual entry point + event loop
   - SDK should: Optional template/helpers for common patterns
   - Keep: Process model and task scheduling

### What NOT to Change

❌ **DO NOT:**
- Add direct framebuffer access for apps
- Break event serialization format
- Change window manager UI
- Add preemption/threading
- Remove single address space model
- Change capability system

✓ **DO:**
- Build on existing patterns
- Maintain backward compatibility
- Preserve security model
- Use existing APIs as foundations
- Enhance developer productivity

## SDK Layering Strategy

### Layer 0: Core System (Unchanged)
```
event_bus_publish/receive
process_create/exit
scheduler_run
wm_create_window/set_content
heap_alloc/free
```

### Layer 1: Serialization (NEW)
```
sdk_event_pack/unpack
sdk_send_custom_event
sdk_receive_custom_event
sdk_event_wait_response
```

### Layer 2: Buffers (NEW)
```
sdk_buffer_init
sdk_buffer_append
sdk_buffer_clear
sdk_buffer_trim
sdk_buffer_wordwrap
```

### Layer 3: Input (NEW)
```
sdk_input_is_navigation
sdk_input_is_select
sdk_input_is_cancel
sdk_input_get_char
sdk_input_get_mouse_pos
```

### Layer 4: UI (NEW)
```
sdk_render_button
sdk_render_list
sdk_render_input_field
sdk_render_table
sdk_render_dialog
```

### Layer 5: Application (NEW)
```
sdk_app_create
sdk_app_template
sdk_app_registry
sdk_app_launch
```

## Example: Console App with SDK

### Without SDK (Current)
```c
// Manual everything
static BOOLEAN my_app_task(void *context) {
    app_instance_t *instance = (app_instance_t *)context;
    
    event_packet_t packet;
    while (event_bus_receive(instance->pid, &packet)) {
        if (packet.code == EVENT_CODE_APP_INPUT) {
            input_event_t input;
            UINT8 *dst = (UINT8 *)&input;
            for (UINTN i = 0; i < sizeof(input_event_t); ++i) {
                dst[i] = packet.payload[i];
            }
            
            if (input.type == INPUT_EVENT_KEY_DOWN) {
                CHAR16 ch = input.data.key.unicode;
                if (ch == 0x0D) {
                    // Execute command
                } else if (ch == 0x08) {
                    if (instance->input_len > 0) instance->input_len--;
                } else if (ch >= 0x20) {
                    if (instance->input_len < 95) {
                        instance->input_line[instance->input_len++] = ch;
                        instance->input_line[instance->input_len] = 0;
                    }
                }
            }
        }
    }
    
    // Rebuild view
    instance->content[0] = 0;
    instance->content_len = 0;
    // ... manual string concatenation ...
    
    wm_set_window_content(instance->window_id, instance->content);
    return TRUE;
}
```

### With SDK (Proposed)
```c
// Higher-level programming
typedef struct {
    CHAR16 input_line[96];
    UINTN input_len;
    CHAR16 history[512];
    UINTN history_len;
} my_app_state_t;

static BOOLEAN on_input(const input_event_t *input, void *state) {
    my_app_state_t *app = (my_app_state_t *)state;
    
    if (!sdk_input_is_char(input)) return FALSE;
    CHAR16 ch = sdk_input_get_char(input);
    
    if (ch == L'\r') {
        execute_command(app->input_line);
        sdk_buffer_append(app->history, 512, &app->history_len, app->input_line);
        app->input_len = 0;
    } else if (ch == 0x08) {
        sdk_buffer_backspace(app->input_line, &app->input_len);
    } else {
        sdk_buffer_append_char(app->input_line, 96, &app->input_len, ch);
    }
    return TRUE;
}

static void on_render(CHAR16 *content, UINTN max_chars, void *state) {
    my_app_state_t *app = (my_app_state_t *)state;
    
    UINTN len = 0;
    sdk_buffer_append(content, max_chars, &len, app->history);
    sdk_buffer_append_char(content, max_chars, &len, L'\n');
    sdk_buffer_append(content, max_chars, &len, L"shell> ");
    sdk_buffer_append(content, max_chars, &len, app->input_line);
}

int main() {
    sdk_app_config_t config = {
        .id = L"myapp",
        .title = L"My Console App",
        .on_input = on_input,
        .on_render = on_render,
        .on_render_interval = 1,  // Every tick
    };
    
    my_app_state_t state = { 0 };
    return sdk_create_console_app(&config, &state) ? 0 : 1;
}
```

## Files Structure for SDK

```
/Users/nandan/dev/mars/
├── os/
│   ├── include/
│   │   ├── sdk/                          (NEW)
│   │   │   ├── sdk_app.h
│   │   │   ├── sdk_buffer.h
│   │   │   ├── sdk_event.h
│   │   │   ├── sdk_input.h
│   │   │   └── sdk_ui.h
│   │   ├── app.h                        (existing)
│   │   ├── event_bus.h                  (existing)
│   │   └── ...
│   └── lib/
│       └── sdk/                          (NEW)
│           ├── sdk_app.c
│           ├── sdk_buffer.c
│           ├── sdk_event.c
│           ├── sdk_input.c
│           └── sdk_ui.c
├── TECHNICAL_OVERVIEW.md                (NEW - this analysis)
├── SDK_CONTEXT.md                       (NEW - this document)
└── AGENTS.md                            (existing guidelines)
```

## Next Steps for SDK Development

1. **Phase 1: Foundation**
   - Implement Layer 1 (Event serialization)
   - Implement Layer 2 (Buffer management)
   - Test with existing apps

2. **Phase 2: Input & UI**
   - Implement Layer 3 (Input processing)
   - Implement Layer 4 (UI components)
   - Create example apps

3. **Phase 3: Application Framework**
   - Implement Layer 5 (App helpers)
   - Create templates for common patterns
   - Document best practices

4. **Phase 4: Testing & Docs**
   - Comprehensive examples
   - Tutorial documentation
   - Performance tuning

## References

- **TECHNICAL_OVERVIEW.md**: Complete system architecture
- **AGENTS.md**: Project development guidelines
- **Source Code**: All implementation files referenced above

---

**Status**: Ready for SDK design and implementation
**Last Updated**: 2026-02-21
