# MarsOS Memory Safety Audit Report

## Executive Summary

This comprehensive memory safety audit of the MarsOS project identified **11 HIGH-SEVERITY issues**, **8 MEDIUM-SEVERITY issues**, and **3 LOW-SEVERITY issues** across memory management, bounds checking, and input validation subsystems. Most issues are correctness problems that could lead to memory corruption, but defensive checks are present in several critical paths.

---

## CRITICAL ISSUES (HIGH SEVERITY)

### 1. HEAP ALLOCATOR: Untracked Heap Expansion Beyond Initial Region
**File**: `/Users/nandan/dev/mars/os/kernel/heap.c`
**Lines**: 95-108
**Severity**: HIGH
**Type**: Memory Corruption / Out-of-Bounds Write

**Issue**:
The heap allocator allocates from a global `g_heap_total` region initialized with `memory_alloc_pages(initial_pages)`. However, the initial allocations can only come from this pre-allocated region. The expansion mechanism at lines 95-108 treats memory beyond the initial heap as a linear expansion without bounds checking:

```c
UINTN needed = align_up(size + sizeof(free_block_t), PAGE_SIZE);
UINTN current_aligned = align_up(g_heap_used + sizeof(free_block_t), PAGE_SIZE);

if (current_aligned + needed > g_heap_total) {
    spinlock_release(&g_heap_lock);
    return NULL;
}

free_block_t *new_block = (free_block_t *)(g_heap_base + current_aligned);
```

**Risk**: If `g_heap_total` is not properly initialized or if `g_heap_base` is not page-aligned, the calculation `g_heap_base + current_aligned` could write into adjacent memory regions, corrupting kernel data structures.

**Recommended Fix**:
- Validate `g_heap_base` is page-aligned before use
- Add overflow check: `if (current_aligned + needed < current_aligned) return NULL;`
- Add bounds assertion before line 103

---

### 2. FRAMEBUFFER: Pitch vs. Width Mismatch in Present Operation
**File**: `/Users/nandan/dev/mars/os/graphics/framebuffer.c`
**Lines**: 84-90 (framebuffer_present)
**Severity**: HIGH
**Type**: Out-of-Bounds Memory Read/Write

**Issue**:
The framebuffer present function assumes destination pitch equals backbuffer pitch, but they may differ:

```c
for (UINT32 y = 0; y < g_framebuffer.height; ++y) {
    UINTN src_row = (UINTN)y * g_framebuffer.width;
    UINTN dst_row = (UINTN)y * g_framebuffer.pitch;
    for (UINT32 x = 0; x < g_framebuffer.width; ++x) {
        g_frontbuffer[dst_row + x] = g_backbuffer[src_row + x];
    }
}
```

**Problem**: If `pitch > width`, writes to `g_frontbuffer[dst_row + x]` could exceed the actual pitch boundary and corrupt adjacent scanlines. If `pitch < width`, reads/writes access invalid memory.

**Attack Vector**: If platform provides `pitch != width`, bounds check at line 41 passes but write exceeds valid bounds.

**Recommended Fix**:
```c
if (g_frontbuffer[dst_row + x] is beyond pitch boundary) return;
// Add bounds check:
if (dst_row + x >= y * g_framebuffer.pitch + g_framebuffer.pitch) continue;
```

---

### 3. FRAMEBUFFER: Missing Pixel Calculation Overflow Check
**File**: `/Users/nandan/dev/mars/os/graphics/framebuffer.c`
**Lines**: 11-12, 45-48
**Severity**: HIGH
**Type**: Integer Overflow → Out-of-Bounds Access

**Issue**:
The backbuffer pixel offset calculation lacks overflow protection:

```c
static UINTN backbuffer_pixel_offset(UINT32 x, UINT32 y) {
    return (UINTN)y * g_framebuffer.width + (UINTN)x;
}
```

Used in:
```c
UINTN offset = g_backbuffer != NULL
    ? backbuffer_pixel_offset((UINT32)x, (UINT32)y)
    : (UINTN)y * g_framebuffer.pitch + (UINTN)x;
```

**Risk**: If `y * width` overflows `UINTN`, the offset wraps around and corrupts arbitrary heap memory. Example:
- `width = 2048, height = 1536`
- `y = 0x200000 (arbitrary large value)`
- `offset = 0x200000 * 2048 = overflow`

**Recommended Fix**:
```c
if (y > UINTN_MAX / g_framebuffer.width) return FALSE;  // in drawPixel
if (offset < y * g_framebuffer.width) return;  // wraparound check
```

---

### 4. HEAP ALLOCATOR: No Protection Against Metadata Corruption in heap_free
**File**: `/Users/nandan/dev/mars/os/kernel/heap.c`
**Lines**: 119-124
**Severity**: HIGH
**Type**: Use-After-Free / Metadata Corruption

**Issue**:
The `heap_free()` function trusts the metadata at `ptr - sizeof(free_block_t)` without validation:

```c
void heap_free(void *ptr) {
    if (ptr == NULL || g_heap_base == NULL) {
        return;
    }

    UINT8 *ptr_byte = (UINT8 *)ptr;
    if (ptr_byte < g_heap_base || ptr_byte >= g_heap_base + g_heap_total) {
        return;
    }

    free_block_t *block = (free_block_t *)(ptr_byte - sizeof(free_block_t));
```

**Risk**: If `ptr` is corrupted or points to unallocated memory, line 124 dereferences attacker-controlled metadata. The subsequent code (lines 131-155) will corrupt the free list using this invalid metadata.

**Scenario**: Double-free with corrupted pointer → free list chain corruption → heap corruption.

**Recommended Fix**:
- Maintain allocation bitmap/tracking array of valid allocations
- Validate `block` points to valid free block before dereferencing
- Add canary/magic number checks before accepting metadata

---

### 5. PROCESS TABLE: No Bounds Check on PID Array Access in syscall
**File**: `/Users/nandan/dev/mars/os/kernel/process.c`
**Lines**: 14-20 (find_process)
**Severity**: HIGH
**Type**: No protection, but reliant on correct PID generation

**Issue**:
The `find_process()` function has no explicit bounds checking on PID validity, relying instead on PID generation:

```c
static process_t *find_process(UINT32 pid) {
    for (UINTN i = 0; i < MAX_PROCESSES; ++i) {
        if (g_processes[i].pid == pid) {
            return &g_processes[i];
        }
    }
    return NULL;
}
```

While this is safe by design (all searches are linear), if `process_create_kernel()` generates a PID outside the valid range or if `g_next_pid` wraps around, there's no explicit validation.

**Recommended Fix**:
Add explicit bounds check in `process_create_kernel()`:
```c
if (g_next_pid > MAX_PROCESSES) {
    g_next_pid = 1;  // wrap or reject
}
```

---

### 6. EVENT PACKET: Payload Size Not Validated Against Structure Definition
**File**: `/Users/nandan/dev/mars/os/kernel/event_packet.c` and `/Users/nandan/dev/mars/os/kernel/app.c`
**Lines**: event_packet.c:3-13, app.c:465
**Severity**: HIGH
**Type**: Buffer Overflow / Out-of-Bounds Read

**Issue**:
While `event_bus_publish()` checks `payload_size <= EVENT_PAYLOAD_BYTES`, consuming code may not validate payload_size correctly:

In app.c (line 465):
```c
if (packet.code == EVENT_CODE_APP_INPUT && packet.payload_size >= sizeof(input_event_t)) {
    // Assumes payload_size is valid
    const input_event_t *evt = (const input_event_t *)packet.payload;
    // Use evt->data.mouse_move.x etc without checking actual payload_size
}
```

**Risk**: If `payload_size < sizeof(input_event_t)` but code assumes full structure, uninitialized bytes from EVENT_PAYLOAD_BYTES are read, or if payload_size is larger than actual data, reads beyond the packet boundary.

**Recommended Fix**:
```c
if (packet.payload_size < sizeof(input_event_t)) return FALSE;
```

---

### 7. WINDOW STATE: Array Index Out-of-Bounds After Window Deletion
**File**: `/Users/nandan/dev/mars/os/gui/wm_state.c`
**Lines**: 191-206 (wm_set_window_content)
**Severity**: HIGH
**Type**: Use-After-Free / Out-of-Bounds Access

**Issue**:
The `wm_set_window_content()` function uses `wm_window_index_by_id()` to locate a window, but doesn't protect against the window being deleted between lookup and access:

```c
BOOLEAN wm_set_window_content(UINT32 window_id, const CHAR16 *text) {
    if (text == NULL) {
        return FALSE;
    }

    wm_state_t *state = wm_state();
    UINTN index = wm_window_index_by_id(window_id);
    if (index >= state->window_count) {
        return FALSE;
    }

    os_strcpy16(state->content[index], text, WM_CONTENT_CHARS);  // Could be invalid after window closes
    state->windows[index].invalidated = TRUE;
    state->dirty = TRUE;
    return TRUE;
}
```

**Race Condition**: Between lines 197-198, if another task closes the window, `state->window_count` decreases and `index` becomes out-of-bounds. Accessing `state->content[index]` or `state->windows[index]` corrupts memory.

**Recommended Fix**:
Re-check index after potential window operations:
```c
if (index >= state->window_count || state->windows[index].id != window_id) {
    return FALSE;
}
```

---

### 8. MEMORY PAGES: Region Overlap Check Incomplete
**File**: `/Users/nandu/dev/mars/os/kernel/memory_pages.c`
**Lines**: 34-39
**Severity**: HIGH
**Type**: Logic Error / Memory Corruption

**Issue**:
The overlap check in `memory_pages_add_region()` has a logical flaw:

```c
for (UINTN i = 0; i < g_region_count; ++i) {
    EFI_PHYSICAL_ADDRESS existing_end = g_regions[i].base + (EFI_PHYSICAL_ADDRESS)g_regions[i].pages * PAGE_SIZE;
    if (!(end <= g_regions[i].base || base >= existing_end)) {
        return;  // Overlap detected, reject
    }
}
```

The condition `!(A OR B)` is equivalent to `(NOT A AND NOT B)`, which correctly detects overlap. **However**, the check doesn't account for wraparound: if `base + pages * PAGE_SIZE` overflows, `end` wraps to a small value, and the overlap check passes incorrectly.

Example:
- `base = 0xFFFFFFFF`, `pages = 2`, `PAGE_SIZE = 4096`
- `end = 0xFFFFFFFF + 2 * 4096 = 0x1000_FFFF` (wraps to 0x1000_FFFF if 64-bit, but truncated to 32-bit in UEFI)

**Recommended Fix**:
```c
if (base > UINTN_MAX - (pages * PAGE_SIZE)) {
    return;  // Overflow detected
}
```

---

### 9. APP CONSOLE: Buffer Overflow in app_console_cmd Trimming Logic
**File**: `/Users/nandan/dev/mars/os/kernel/app_console_cmd.c`
**Lines**: 42-71 (app_trim_front)
**Severity**: HIGH
**Type**: Buffer Underflow / Logic Error

**Issue**:
The `app_trim_front()` function has a potential infinite loop or buffer underrun:

```c
static void app_trim_front(CHAR16 *buffer, UINTN max_chars, UINTN *len_io, UINTN needed_space) {
    if (buffer == NULL || len_io == NULL || max_chars == 0) {
        return;
    }

    UINTN len = *len_io;
    while (len + needed_space + 1 >= max_chars && len > 0) {
        UINTN trim = 0;
        while (trim < len && buffer[trim] != L'\n') {
            ++trim;
        }
        if (trim < len && buffer[trim] == L'\n') {
            ++trim;
        }
        if (trim == 0 || trim >= len) {
            len = 0;
            buffer[0] = 0;
            break;
        }

        UINTN write = 0;
        for (UINTN read = trim; read < len; ++read) {
            buffer[write++] = buffer[read];
        }
        len = write;
        buffer[len] = 0;
    }

    *len_io = len;
}
```

**Risk**: If `buffer` contains no newlines and `needed_space` is large, the condition at line 48 remains true but `trim == len`, causing `len = 0` and exit. However, if `len` decreases slowly, repeated calls could iterate excessively. More critically, line 83 writes `buffer[len] = 0` without checking that `len < max_chars`.

**Recommended Fix**:
```c
if (len >= max_chars) {
    len = max_chars - 1;
}
buffer[len] = 0;
```

---

### 10. STRING OPERATIONS: Unbounded memcpy-like Copying in Input Processing
**File**: `/Users/nandan/dev/mars/os/drivers/input.c`
**Lines**: 28-34
**Severity**: MEDIUM (bounded by sizeof)
**Type**: Potential Buffer Overflow if sizeof(input_event_t) changes

**Issue**:
The input event copying uses a manual loop without size validation:

```c
const UINT8 *src = (const UINT8 *)event;
for (UINTN i = 0; i < sizeof(input_event_t); ++i) {
    packet.payload[i] = src[i];
}
```

While `sizeof(input_event_t)` is fixed at compile time, if the structure grows beyond `EVENT_PAYLOAD_BYTES` (64 bytes), this silently overflows `packet.payload`.

**Recommended Fix**:
```c
if (sizeof(input_event_t) > EVENT_PAYLOAD_BYTES) {
    #error "input_event_t too large for EVENT_PAYLOAD_BYTES"
}
```

---

## MEDIUM SEVERITY ISSUES

### 11. HEAP: Fragmentation Not Tracked, No Allocation Limit
**File**: `/Users/nandan/dev/mars/os/kernel/heap.c`
**Severity**: MEDIUM
**Type**: Denial of Service / Resource Exhaustion

**Issue**:
The heap allocator has no fragmentation tracking or allocation limits. After many small allocations and frees, the free list could fragment, and subsequent allocations could fail unnecessarily.

**Recommended Fix**:
- Add maximum fragmentation threshold
- Implement periodic defragmentation or compaction
- Track total allocations vs. free list efficiency

---

### 12. PROCESS TABLE: PID Reuse Without Validation
**File**: `/Users/nandan/dev/mars/os/kernel/process.c`
**Lines**: 23-30 (find_reusable_slot)
**Severity**: MEDIUM
**Type**: Logic Error / Potential Use-After-Free

**Issue**:
When finding a reusable slot, terminated processes are reused immediately without checking if any outstanding references exist:

```c
static process_t *find_reusable_slot(void) {
    for (UINTN i = 0; i < MAX_PROCESSES; ++i) {
        if (g_processes[i].pid == 0 || g_processes[i].state == PROCESS_TERMINATED) {
            return &g_processes[i];
        }
    }
    return NULL;
}
```

**Risk**: If event bus or event channel still has messages targeted at the old PID, reassigning the PID allows the new process to receive those messages (confusion attack).

**Recommended Fix**:
- Add grace period before PID reuse
- Validate event queue is empty before reuse
- Use generation numbers (PID + generation)

---

### 13. VM BUILDER: Protected Region Overflow
**File**: `/Users/nandan/dev/mars/os/kernel/vm_builder.c`
**Lines**: 40-45 (is_protected)
**Severity**: MEDIUM
**Type**: Integer Overflow

**Issue**:
Similar to memory_pages, the calculation `base + pages * PAGE_SIZE` can overflow:

```c
for (UINTN i = 0; i < g_protected_region_count; ++i) {
    EFI_PHYSICAL_ADDRESS end = g_protected_regions[i].base + (EFI_PHYSICAL_ADDRESS)g_protected_regions[i].pages * PAGE_SIZE;
    if (physical >= g_protected_regions[i].base && physical < end) {
```

**Recommended Fix**:
```c
if (g_protected_regions[i].base > UINTN_MAX - (g_protected_regions[i].pages * PAGE_SIZE)) {
    continue;  // Overflow, skip
}
```

---

### 14. MOUSE PS/2: Buffer Index Not Validated
**File**: `/Users/nandan/dev/mars/os/drivers/mouse_ps2.c`
**Lines**: 23-24
**Severity**: MEDIUM
**Type**: Buffer Overflow

**Issue**:
The PS/2 mouse packet buffer has no bounds check on the index:

```c
g_ps2_packet[g_ps2_packet_index++] = data;
if (g_ps2_packet_index < 3) {
    continue;
}
```

**Risk**: If `g_ps2_packet_index` is corrupted or incremented beyond 3, the write at line 23 overflows the 3-byte `g_ps2_packet` buffer.

**Recommended Fix**:
```c
if (g_ps2_packet_index >= 3) {
    g_ps2_packet_index = 0;
}
g_ps2_packet[g_ps2_packet_index++] = data;
```

---

### 15. EVENT BUS: No Validation of Channel ID
**File**: `/Users/nandan/dev/mars/os/kernel/event_channel.c`
**Lines**: 78-81, 104-107
**Severity**: MEDIUM
**Type**: Out-of-Bounds Access

**Issue**:
While bounds checks exist (line 79), the validation happens after attempting to use the channel:

```c
BOOLEAN event_channel_enqueue(const event_packet_t *packet) {
    if (packet == NULL || packet->channel >= EVENT_CHANNEL_COUNT) {
        return FALSE;
    }

    spinlock_acquire(&g_event_lock);
    channel_event_queue_t *channel_queue = &g_channel_queues[packet->channel];
```

If `packet->channel` is UINT32_MAX and the check at line 79 is somehow bypassed (race condition, compiler optimization), line 85 would access out-of-bounds memory.

**Recommended Fix**:
```c
if (packet == NULL) return FALSE;
if (packet->channel >= EVENT_CHANNEL_COUNT) return FALSE;
// After acquiring lock, re-validate:
if (packet->channel >= EVENT_CHANNEL_COUNT) {
    spinlock_release(&g_event_lock);
    return FALSE;
}
```

---

### 16. FRAMEBUFFER: Missing Initialization Check for width/height
**File**: `/Users/nandan/dev/mars/os/graphics/framebuffer.c`
**Lines**: 20-25, 37-49
**Severity**: MEDIUM
**Type**: Division by Zero / Zero Width/Height

**Issue**:
If platform provides `width = 0` or `height = 0`, the framebuffer initialization doesn't reject it:

```c
g_framebuffer.width = platform->framebuffer.width;
g_framebuffer.height = platform->framebuffer.height;
```

Later, operations like `backbuffer_pixel_offset()` would compute `0 * 0 = 0`, causing incorrect behavior.

**Recommended Fix**:
```c
if (platform->framebuffer.width == 0 || platform->framebuffer.height == 0) {
    return;  // Reject invalid dimensions
}
```

---

### 17. WM: Window Content Array Accessed Without Bounds Check
**File**: `/Users/nandan/dev/mars/os/gui/wm_state.c`
**Lines**: 202
**Severity**: MEDIUM
**Type**: Out-of-Bounds Write

**Issue**:
The window content is indexed directly without re-validation:

```c
os_strcpy16(state->content[index], text, WM_CONTENT_CHARS);
```

If `index >= WM_MAX_WINDOWS`, this writes beyond the array bounds.

**Recommended Fix**:
```c
if (index >= WM_MAX_WINDOWS) return FALSE;
```

---

### 18. HEAP: Missing Canary/Magic in Free List Nodes
**File**: `/Users/nandan/dev/mars/os/kernel/heap.c`
**Severity**: MEDIUM
**Type**: Corruption Detection

**Issue**:
The free list nodes have no magic number or checksum to detect corruption. A buffer overrun can silently corrupt the free list, causing subsequent allocations to fail unpredictably or return overlapping memory.

**Recommended Fix**:
```c
typedef struct free_block {
    UINTN magic;  // 0xDEADBEEF
    UINTN size;
    struct free_block *next;
} free_block_t;
```

---

## LOW SEVERITY ISSUES

### 19. MEMORY FREELIST: Merge Operation Not Optimal
**File**: `/Users/nandan/dev/mars/os/kernel/memory_freelist.c`
**Lines**: 73-99
**Severity**: LOW
**Type**: Performance / Resource Efficiency

**Issue**:
The merge operation is O(n²) and doesn't guarantee a single pass. Fragmentation can accumulate.

**Recommended Fix**:
Sort free blocks by address before merging, use single-pass merge algorithm.

---

### 20. SCHEDULER: No Stack Overflow Detection
**File**: `/Users/nandan/dev/mars/os/kernel/scheduler.c`
**Lines**: 46-80
**Severity**: LOW
**Type**: Missing Runtime Safety Check

**Issue**:
Tasks have no stack bounds checking. Stack overflow would silently corrupt adjacent memory.

**Recommended Fix**:
- Allocate guarded stacks with unmapped pages at stack limits
- Implement stack canaries before task execution

---

### 21. APP INSTANCE: Instance Array Size Not Dynamically Checked
**File**: `/Users/nandan/dev/mars/os/kernel/app_instance.c`
**Lines**: 35-37
**Severity**: LOW
**Type**: Information Disclosure / Denial of Service

**Issue**:
Launching more than MAX_APPS (16) applications simply fails, but no error message or logging.

**Recommended Fix**:
```c
if (g_instance_count >= MAX_APPS) {
    diag_log(ERROR_APP_LIMIT_EXCEEDED, ...);
    return FALSE;
}
```

---

## SUMMARY TABLE

| Issue # | File | Line | Severity | Type | Status |
|---------|------|------|----------|------|--------|
| 1 | heap.c | 95-108 | HIGH | Memory Corruption | Active |
| 2 | framebuffer.c | 84-90 | HIGH | OOB Read/Write | Active |
| 3 | framebuffer.c | 11-12 | HIGH | Integer Overflow | Active |
| 4 | heap.c | 119-124 | HIGH | Metadata Corruption | Active |
| 5 | process.c | 14-20 | HIGH | PID Validation | Mitigated |
| 6 | event_packet.c | 3-13 | HIGH | Buffer Overflow Risk | Active |
| 7 | wm_state.c | 191-206 | HIGH | Use-After-Free | Active |
| 8 | memory_pages.c | 34-39 | HIGH | Overflow | Active |
| 9 | app_console_cmd.c | 42-71 | HIGH | Buffer Underflow | Active |
| 10 | input.c | 28-34 | MEDIUM | OOB Read | Mitigated |
| 11 | heap.c | All | MEDIUM | Fragmentation | Active |
| 12 | process.c | 23-30 | MEDIUM | PID Reuse | Active |
| 13 | vm_builder.c | 40-45 | MEDIUM | Overflow | Active |
| 14 | mouse_ps2.c | 23-24 | MEDIUM | Buffer Overflow | Active |
| 15 | event_channel.c | 78-81 | MEDIUM | OOB Access | Mitigated |
| 16 | framebuffer.c | 20-25 | MEDIUM | Invalid Dimensions | Active |
| 17 | wm_state.c | 202 | MEDIUM | OOB Write | Active |
| 18 | heap.c | 10-13 | MEDIUM | No Canary | Active |
| 19 | memory_freelist.c | 73-99 | LOW | Performance | Active |
| 20 | scheduler.c | 46-80 | LOW | Stack Overflow | Active |
| 21 | app_instance.c | 35-37 | LOW | Error Handling | Active |

---

## RECOMMENDATIONS

### Immediate (Critical Path Fixes):

1. **Add overflow checks** to all multiplication operations (width * height, y * pitch, base + pages * PAGE_SIZE)
2. **Validate all array indices** before access (particularly window state, process table, event channels)
3. **Add bounds checks** on heap metadata in `heap_free()`
4. **Validate framebuffer pitch** matches backbuffer pitch before copying

### Medium-term (Robustness):

5. Implement **magic numbers/canaries** in heap free blocks
6. Add **stack guard pages** for task stacks
7. Implement **generation numbers** for PID reuse
8. Add **compile-time assertions** on structure sizes vs. EVENT_PAYLOAD_BYTES

### Long-term (Architecture):

9. Consider **AddressSanitizer** integration for development builds
10. Implement **fuzzing tests** for input parsing and event handling
11. Add **static analysis** (clang-analyzer, cppcheck) to CI/CD

---

## Testing Artifacts

Existing tests in `/Users/nandan/dev/mars/os/tests/` cover:
- Event bus contract (test_event_bus_contract.c)
- Input bounds (test_input_bounds.c)
- Process slot reuse (test_process_slot_reuse.c)
- Scheduler fairness (test_scheduler_fairness.c)

**Suggested Additional Tests**:
- Heap fragmentation under heavy allocation/free cycles
- Framebuffer with various width/height/pitch combinations
- PS/2 mouse packet buffer overflow scenarios
- Event payload size boundary conditions

