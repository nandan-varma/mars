# MarsOS Memory Safety Issues - Quick Reference

## HIGH SEVERITY - Fix First

### ISSUE #1: Heap Allocation Overflow
- **File**: `kernel/heap.c:95-108`
- **Function**: `heap_alloc()`
- **Problem**: No overflow check when computing `g_heap_base + current_aligned`
- **Impact**: Writes to adjacent memory regions, kernel data corruption
- **Fix**: Add `if (current_aligned + needed < current_aligned) return NULL;`

### ISSUE #2: Framebuffer Pitch Mismatch
- **File**: `graphics/framebuffer.c:84-90`
- **Function**: `framebuffer_present()`
- **Problem**: Assumes `pitch == width`, but may differ on some platforms
- **Impact**: Out-of-bounds writes corrupt adjacent scanlines
- **Fix**: Validate `pitch >= width` before copying, or add bounds check per pixel

### ISSUE #3: Framebuffer Offset Overflow
- **File**: `graphics/framebuffer.c:11-12, 45-48`
- **Function**: `backbuffer_pixel_offset()`
- **Problem**: `y * width` can overflow UINTN
- **Impact**: Offset wraps to small value, corrupts arbitrary heap memory
- **Fix**: Add `if (y > UINTN_MAX / width) return` check in drawPixel

### ISSUE #4: Heap Free Without Metadata Validation
- **File**: `kernel/heap.c:119-124`
- **Function**: `heap_free()`
- **Problem**: Dereferences metadata at `ptr - sizeof(free_block_t)` without validation
- **Impact**: Corrupted pointer → free list chain corruption
- **Fix**: Add allocation bitmap or magic number validation before dereferencing

### ISSUE #5: PID Generation Unbounded
- **File**: `kernel/process.c:64`
- **Function**: `process_create_kernel()`
- **Problem**: `g_next_pid++` can overflow to 0, colliding with invalid PIDs
- **Impact**: Process lookup returns wrong process if PID wraps
- **Fix**: Add `if (g_next_pid > MAX_PROCESSES) g_next_pid = 1;` wrap logic

### ISSUE #6: Event Payload Not Validated
- **File**: `kernel/app.c:465`
- **Function**: Event handling
- **Problem**: Assumes `payload_size >= sizeof(input_event_t)` without checking
- **Impact**: Reads uninitialized bytes from EVENT_PAYLOAD_BYTES array
- **Fix**: Add explicit check `if (payload_size != sizeof(input_event_t)) return;`

### ISSUE #7: Window Index Race Condition
- **File**: `gui/wm_state.c:191-206`
- **Function**: `wm_set_window_content()`
- **Problem**: Window could close between lookup and access
- **Impact**: Out-of-bounds array access corrupts memory
- **Fix**: Re-validate `index < state->window_count && state->windows[index].id == window_id`

### ISSUE #8: Memory Region Overlap Wraparound
- **File**: `kernel/memory_pages.c:34-39`
- **Function**: `memory_pages_add_region()`
- **Problem**: `base + pages * PAGE_SIZE` can overflow, bypassing overlap check
- **Impact**: Overlapping memory regions registered, corruption on allocation
- **Fix**: Add `if (base > UINTN_MAX - (pages * PAGE_SIZE)) return;`

### ISSUE #9: App Trim Buffer Underflow
- **File**: `kernel/app_console_cmd.c:42-71`
- **Function**: `app_trim_front()`
- **Problem**: Writes `buffer[len] = 0` without checking `len < max_chars`
- **Impact**: Out-of-bounds null terminator writes
- **Fix**: Add `if (len >= max_chars) len = max_chars - 1;` before writing terminator

---

## MEDIUM SEVERITY - Fix Soon

### ISSUE #10: Input Event Copy Without Size Assert
- **File**: `drivers/input.c:28-34`
- **Problem**: Copies `sizeof(input_event_t)` without compile-time check against EVENT_PAYLOAD_BYTES
- **Fix**: Add `#if sizeof(input_event_t) > EVENT_PAYLOAD_BYTES #error "overflow"`

### ISSUE #11: Heap Fragmentation Untracked
- **File**: `kernel/heap.c` (entire)
- **Problem**: No defragmentation, free list can become highly fragmented
- **Impact**: Allocation failures despite available memory
- **Fix**: Implement compaction or sorted free list merge algorithm

### ISSUE #12: PID Reuse Without Validation
- **File**: `kernel/process.c:23-30`
- **Problem**: Immediately reuses terminated PIDs without draining event queues
- **Impact**: Old process messages delivered to new process (confusion attack)
- **Fix**: Add grace period, validate event queue empty before reuse

### ISSUE #13: VM Protected Region Overflow
- **File**: `kernel/vm_builder.c:40-45`
- **Problem**: `base + pages * PAGE_SIZE` can overflow like Issue #8
- **Fix**: Add overflow check before subtraction

### ISSUE #14: PS/2 Mouse Buffer Overflow
- **File**: `drivers/mouse_ps2.c:23-24`
- **Problem**: No bounds check on `g_ps2_packet_index++` increment
- **Impact**: Writes beyond 3-byte buffer if index corrupted
- **Fix**: Add `if (g_ps2_packet_index >= 3) g_ps2_packet_index = 0;`

### ISSUE #15: Event Channel ID Not Re-validated
- **File**: `kernel/event_channel.c:78-85`
- **Problem**: Bounds check before lock but not after (race window)
- **Impact**: Theoretical out-of-bounds access if check bypassed
- **Fix**: Re-validate after acquiring spinlock

### ISSUE #16: Framebuffer Zero Dimensions Not Rejected
- **File**: `graphics/framebuffer.c:20-25`
- **Problem**: Accepts `width == 0 or height == 0`
- **Impact**: Division by zero or invalid pixel calculations
- **Fix**: Add `if (width == 0 || height == 0) return;`

### ISSUE #17: Window Content Array Out-of-Bounds
- **File**: `gui/wm_state.c:202`
- **Problem**: Directly indexes `state->content[index]` without bounds check
- **Impact**: Out-of-bounds write if index >= WM_MAX_WINDOWS
- **Fix**: Add `if (index >= WM_MAX_WINDOWS) return FALSE;`

### ISSUE #18: Heap Free Block No Canary
- **File**: `kernel/heap.c:10-13`
- **Problem**: Free list nodes have no magic/checksum
- **Impact**: Buffer overrun silently corrupts free list
- **Fix**: Add `UINTN magic;  // 0xDEADBEEF` to struct and check on use

---

## LOW SEVERITY - Optimize

### ISSUE #19: Memory Freelist Merge O(n²)
- **File**: `kernel/memory_freelist.c:73-99`
- **Problem**: Inefficient merge algorithm
- **Fix**: Sort by address, single-pass merge

### ISSUE #20: No Stack Guard Pages
- **File**: `kernel/scheduler.c`
- **Problem**: Task stacks have no overflow detection
- **Fix**: Allocate with unmapped pages at limits

### ISSUE #21: App Instance Limit Silent Fail
- **File**: `kernel/app_instance.c:35-37`
- **Problem**: Launching >MAX_APPS silently fails without logging
- **Fix**: Add `diag_log()` call

---

## Critical Files by Risk

| File | Issues | Critical |
|------|--------|----------|
| kernel/heap.c | 1,4,11,18 | YES - Allocator corruption |
| graphics/framebuffer.c | 2,3,16 | YES - Arbitrary writes |
| gui/wm_state.c | 7,17 | YES - State corruption |
| kernel/process.c | 5,12 | YES - Process safety |
| kernel/event_channel.c | 15 | YES - Event safety |
| kernel/memory_pages.c | 8 | YES - Boot memory |
| kernel/app_console_cmd.c | 9 | YES - Stack safety |
| kernel/app.c | 6 | MEDIUM - Event validation |
| drivers/mouse_ps2.c | 14 | MEDIUM - Driver safety |
| kernel/vm_builder.c | 13 | MEDIUM - VM safety |

---

## Testing Checklist

- [ ] Heap allocation with 0 pages
- [ ] Framebuffer with width=0, height=0, pitch<width
- [ ] Memory region with base near UINTN_MAX
- [ ] Process creation > MAX_PROCESSES times
- [ ] Mouse PS/2 data burst (>64 bytes)
- [ ] Event payload size = 0, 64, 65
- [ ] Window creation, close, access sequence
- [ ] App console with >512 character buffer
- [ ] Heap free with corrupted pointers
- [ ] PID counter wrap around

---

## Compile-Time Assertions to Add

```c
// In include/event_bus.h
_Static_assert(sizeof(input_event_t) <= EVENT_PAYLOAD_BYTES, "input_event_t too large");

// In kernel/heap.c
_Static_assert(sizeof(free_block_t) >= 16, "free_block_t too small for metadata");

// In gui/wm_internal.h
_Static_assert(WM_MAX_WINDOWS <= 256, "WM_MAX_WINDOWS sanity check");
```
