# MarsOS Security Audit - Recommended Code Patches

This document provides concrete code patches for the 4 CRITICAL vulnerabilities identified in the security audit.

## CRITICAL FIX #1: PID Reuse Race Condition

**File**: `os/kernel/process.c`  
**Lines**: 92-115  
**Severity**: CRITICAL

### Problem
```c
void process_exit(UINT32 pid, INT32 exit_code) {
    spinlock_acquire(&g_process_lock);
    
    process_t *process = find_process(pid);
    if (process == NULL) {
        spinlock_release(&g_process_lock);
        return;
    }
    
    process->state = PROCESS_TERMINATED;
    process->exit_code = exit_code;
    process->pid = 0;                    // <-- PID cleared INSIDE lock
    UINT32 task_id = process->task_id;
    process->task_id = 0;
    EFI_PHYSICAL_ADDRESS vm_root = process->vm_root;
    process->vm_root = 0;
    
    spinlock_release(&g_process_lock);   // <-- Lock released
    
    scheduler_stop_task(task_id);
    if (vm_root != 0 && vm_root != vm_pml4_physical()) {
        (void)vm_release_address_space(vm_root);
    }
    event_bus_unregister_process(pid);   // <-- Called OUTSIDE lock
}
```

The window between line 109 (release) and 115 (unregister) allows PID reuse while event queue still exists.

### Solution
```c
void process_exit(UINT32 pid, INT32 exit_code) {
    spinlock_acquire(&g_process_lock);
    
    process_t *process = find_process(pid);
    if (process == NULL) {
        spinlock_release(&g_process_lock);
        return;
    }
    
    process->state = PROCESS_TERMINATED;
    process->exit_code = exit_code;
    // DO NOT clear pid yet - keep it valid for unregister
    UINT32 task_id = process->task_id;
    process->task_id = 0;
    EFI_PHYSICAL_ADDRESS vm_root = process->vm_root;
    process->vm_root = 0;
    
    // MOVED: Call unregister while holding lock
    event_bus_unregister_process(pid);
    
    // NOW clear pid after event bus is cleaned up
    process->pid = 0;
    
    spinlock_release(&g_process_lock);
    
    // All remaining operations happen with pid already cleared
    scheduler_stop_task(task_id);
    if (vm_root != 0 && vm_root != vm_pml4_physical()) {
        (void)vm_release_address_space(vm_root);
    }
}
```

---

## CRITICAL FIX #2: TOCTOU Race in Window Input Routing

**File**: `os/gui/wm_input.c`  
**Lines**: 8-46  
**Severity**: CRITICAL

### Problem
```c
static void forward_input_to_focused_window(const input_event_t *event) {
    wm_state_t *state = wm_state();
    
    if (event == NULL || state->focused_window == 0) {
        return;
    }
    
    wm_window_t *window = wm_find_window(state->focused_window);
    if (window == NULL || window->owner_pid == 0) {
        return;
    }
    
    if (!process_is_running(window->owner_pid)) {  // <-- CHECK
        // ... cleanup ...
        return;
    }
    
    event_packet_t packet;
    // ... build packet ...
    packet.target_pid = window->owner_pid;  // <-- USE (can race)
    // ... more setup ...
    (void)event_bus_publish(&packet);
}
```

Process can exit between the check (line 20) and use (line 33).

### Solution (Option 1: Atomic Snapshot)
```c
static void forward_input_to_focused_window(const input_event_t *event) {
    wm_state_t *state = wm_state();
    
    if (event == NULL || state->focused_window == 0) {
        return;
    }
    
    wm_window_t *window = wm_find_window(state->focused_window);
    if (window == NULL || window->owner_pid == 0) {
        return;
    }
    
    // ATOMICALLY capture owner_pid and validate
    UINT32 owner_pid = window->owner_pid;  // Snapshot
    
    if (!process_is_running(owner_pid)) {
        window->visible = FALSE;
        window->invalidated = TRUE;
        state->focused_window = 0;
        state->active_window = 0;
        state->dirty = TRUE;
        return;
    }
    
    // Use the captured pid consistently
    event_packet_t packet;
    packet.channel = EVENT_CHANNEL_APP;
    packet.code = EVENT_CODE_APP_INPUT;
    packet.source_pid = 0;
    packet.target_pid = owner_pid;  // Use snapshot
    packet.target_window = window->id;
    packet.payload_size = sizeof(input_event_t);
    
    const UINT8 *src = (const UINT8 *)event;
    for (UINTN i = 0; i < sizeof(input_event_t); ++i) {
        packet.payload[i] = src[i];
    }
    for (UINTN i = sizeof(input_event_t); i < EVENT_PAYLOAD_BYTES; ++i) {
        packet.payload[i] = 0;
    }
    
    (void)event_bus_publish(&packet);
}
```

### Solution (Option 2: Hold Lock)
If WM state needs additional locking, add a spinlock to wm_state_t and acquire it during this critical section.

---

## CRITICAL FIX #3: PS/2 Buffer Overflow

**File**: `os/drivers/mouse_ps2.c`  
**Lines**: 3-73  
**Severity**: CRITICAL

### Problem
```c
UINTN poll_ps2_mouse(input_event_t *events_out, UINTN max_events) {
    if (!g_ps2_enabled || events_out == NULL || max_events == 0) {
        return 0;
    }
    
    UINTN event_count = 0;
    UINTN bytes_budget = 64;
    
    while (bytes_budget-- > 0 && (io_in8(0x64) & 0x01)) {
        UINT8 status = io_in8(0x64);
        UINT8 data = io_in8(0x60);
        
        if ((status & 0x20) == 0) {
            continue;
        }
        
        if (g_ps2_packet_index == 0 && (data & 0x08) == 0) {
            continue;
        }
        
        g_ps2_packet[g_ps2_packet_index++] = data;  // <-- Can overflow
        if (g_ps2_packet_index < 3) {               // <-- Wrong check
            continue;
        }
        
        g_ps2_packet_index = 0;
        // ... process packet ...
    }
}
```

The index increments (line 23) BEFORE the bounds check (line 24), allowing index=3,4,5,...

### Solution
```c
UINTN poll_ps2_mouse(input_event_t *events_out, UINTN max_events) {
    if (!g_ps2_enabled || events_out == NULL || max_events == 0) {
        return 0;
    }
    
    UINTN event_count = 0;
    UINTN bytes_budget = 64;
    
    while (bytes_budget-- > 0 && (io_in8(0x64) & 0x01)) {
        UINT8 status = io_in8(0x64);
        UINT8 data = io_in8(0x60);
        
        if ((status & 0x20) == 0) {
            continue;
        }
        
        if (g_ps2_packet_index == 0 && (data & 0x08) == 0) {
            continue;
        }
        
        // CHANGED: Check BEFORE writing, not after
        if (g_ps2_packet_index >= 3) {  // <-- Changed from <
            // Packet is complete, process it
            g_ps2_packet_index = 0;
            
            INT32 dx = (INT8)g_ps2_packet[1];
            INT32 dy = -(INT8)g_ps2_packet[2];
            
            if (dx != 0 || dy != 0) {
                INT32 new_x = g_mouse_x + dx;
                INT32 new_y = g_mouse_y + dy;
                if (dx > 0 && new_x < g_mouse_x) {
                    new_x = INT32_MAX;
                } else if (dx < 0 && new_x > g_mouse_x) {
                    new_x = INT32_MIN;
                }
                if (dy > 0 && new_y < g_mouse_y) {
                    new_y = INT32_MAX;
                } else if (dy < 0 && new_y > g_mouse_y) {
                    new_y = INT32_MIN;
                }
                g_mouse_x = clamp(new_x, 0, g_screen_w - 1);
                g_mouse_y = clamp(new_y, 0, g_screen_h - 1);
                if (event_count < max_events) {
                    events_out[event_count].type = INPUT_EVENT_MOUSE_MOVE;
                    events_out[event_count].data.mouse_move.dx = dx;
                    events_out[event_count].data.mouse_move.dy = dy;
                    events_out[event_count].data.mouse_move.x = g_mouse_x;
                    events_out[event_count].data.mouse_move.y = g_mouse_y;
                    ++event_count;
                }
            }
            
            BOOLEAN left_now = (g_ps2_packet[0] & 0x01) ? TRUE : FALSE;
            if (left_now != g_left_down && event_count < max_events) {
                events_out[event_count].type = left_now ? INPUT_EVENT_MOUSE_BUTTON_DOWN : INPUT_EVENT_MOUSE_BUTTON_UP;
                events_out[event_count].data.mouse_button.left = left_now;
                events_out[event_count].data.mouse_button.right = (g_ps2_packet[0] & 0x02) ? TRUE : FALSE;
                ++event_count;
                g_left_down = left_now;
            }
            
            if (event_count >= max_events) {
                break;
            }
            
            // Start collecting next packet
            continue;
        }
        
        // Append byte to current packet
        g_ps2_packet[g_ps2_packet_index++] = data;
    }
    
    return event_count;
}
```

---

## CRITICAL FIX #4: Capability Bypass in Syscall

**File**: `os/kernel/syscall.c`  
**Lines**: 34-69  
**Severity**: CRITICAL

### Problem
```c
static UINT64 syscall_send_event_handler(UINT64 packet_ptr, UINT64 b, UINT64 c, UINT64 d) {
    const event_packet_t *packet = (const event_packet_t *)(UINTN)packet_ptr;
    if (packet == NULL) {
        return 1;
    }
    
    if ((packet_ptr & (sizeof(UINTN) - 1)) != 0) {
        return 4;
    }
    
    if (packet->channel >= EVENT_CHANNEL_COUNT || packet->payload_size > EVENT_PAYLOAD_BYTES) {
        return 5;
    }
    
    UINT32 pid = process_current_pid();
    if (pid == 0) {
        return 6;
    }
    
    if (packet->source_pid != 0 && packet->source_pid != pid) {
        return 7;
    }
    
    UINT32 caps = process_capabilities(pid);
    if ((caps & CAP_SYSTEM) == 0 && packet->channel == EVENT_CHANNEL_SYSTEM) {
        return 2;  // <-- ONLY blocks SYSTEM channel
    }
    
    event_packet_t sanitized = *packet;
    sanitized.source_pid = pid;
    return event_bus_publish(&sanitized) ? 0 : 3;
}
```

Unprivileged processes can send to EVENT_CHANNEL_APP and reach privileged processes.

### Solution
```c
static UINT64 syscall_send_event_handler(UINT64 packet_ptr, UINT64 b, UINT64 c, UINT64 d) {
    (void)b;
    (void)c;
    (void)d;
    
    const event_packet_t *packet = (const event_packet_t *)(UINTN)packet_ptr;
    if (packet == NULL) {
        return 1;
    }
    
    if ((packet_ptr & (sizeof(UINTN) - 1)) != 0) {
        return 4;
    }
    
    if (packet->channel >= EVENT_CHANNEL_COUNT || packet->payload_size > EVENT_PAYLOAD_BYTES) {
        return 5;
    }
    
    UINT32 pid = process_current_pid();
    if (pid == 0) {
        return 6;
    }
    
    if (packet->source_pid != 0 && packet->source_pid != pid) {
        return 7;
    }
    
    UINT32 caps = process_capabilities(pid);
    
    // ADDED: Block all targeted messaging without CAP_SYSTEM
    if ((caps & CAP_SYSTEM) == 0 && packet->target_pid != 0) {
        return 2;  // Deny targeted messages without privilege
    }
    
    // EXISTING: Block system channel messages
    if ((caps & CAP_SYSTEM) == 0 && packet->channel == EVENT_CHANNEL_SYSTEM) {
        return 2;
    }
    
    event_packet_t sanitized = *packet;
    sanitized.source_pid = pid;
    return event_bus_publish(&sanitized) ? 0 : 3;
}
```

---

## Testing the Patches

### Test 1: PID Reuse Race (Critical Fix #1)
```c
// Create rapid process cycles and send input
for (int i = 0; i < 1000; i++) {
    uint32_t pid = process_create_kernel(...);
    // Send mouse input
    // Kill process
    // Verify input didn't leak to new process
}
```

### Test 2: TOCTOU Race (Critical Fix #2)
```c
// Thread 1: Send input in loop
// Thread 2: Rapidly create/kill focused window owner
// Verify: Input never reaches wrong process
```

### Test 3: PS/2 Overflow (Critical Fix #3)
```c
// Send malformed PS/2 packets:
// - g_ps2_packet_index=2, then 4+ data bytes
// - Reserved bits set
// - Invalid flag combinations
// Verify: No buffer overflow, no memory corruption
```

### Test 4: Capability Bypass (Critical Fix #4)
```c
// Unprivileged process sends:
event_packet_t packet;
packet.channel = EVENT_CHANNEL_APP;           // Not SYSTEM
packet.target_pid = APP_MANAGER_PID;          // Privileged
packet.code = EVENT_CODE_APP_LAUNCH_REQUEST;
memcpy(packet.payload, L"evil", 4);

syscall_send_event(&packet, ...);
// Should return 2 (DENIED) after fix
// Before fix, would succeed and launch malicious app
```

---

## Implementation Timeline

- **Day 1**: Critical Fix #3 (PS/2 overflow) - Safety critical
- **Day 1-2**: Critical Fix #1 (PID race) - Input isolation
- **Day 2-3**: Critical Fix #4 (Capability) - Privilege escalation
- **Day 3**: Critical Fix #2 (TOCTOU) - Input routing safety
- **Day 4-5**: Testing & validation of all patches

---

## Verification Checklist

- [ ] PS/2 buffer index never exceeds 2
- [ ] process_exit() holds spinlock during event_bus_unregister_process()
- [ ] WM input uses atomic snapshot of owner_pid
- [ ] Unprivileged processes denied from sending targeted events
- [ ] All critical fixes compile without warnings
- [ ] Unit tests pass for each vulnerability
- [ ] Integration tests verify no regressions
- [ ] Security audit confirms fixes address root causes

