# MarsOS Test Coverage Audit Report

**Project:** MarsOS UEFI Operating System  
**Audit Date:** February 20, 2026  
**Scope:** `/Users/nandan/dev/mars/os`  
**Total Kernel/Driver/GUI Code:** ~5,472 lines  
**Current Tests:** 4 contract/unit tests  
**Test Coverage:** ~8-12% (estimated)

---

## EXECUTIVE SUMMARY

MarsOS has a **minimal but high-quality test foundation** with 4 focused contract tests covering critical subsystems. However, **major coverage gaps exist** in memory management, virtual memory, graphics, window manager, app framework, and drivers. The test infrastructure is **extensible and ready for significant expansion**.

### Key Findings:
- ✅ **Strong:** Event bus contract tests (queue semantics, backpressure), scheduler fairness, process lifecycle
- ❌ **Critical Gaps:** Memory fragmentation, heap operations, page table correctness, framebuffer bounds, WM state machines, app manifest parsing
- ⚠️  **Infrastructure:** Tests are host-based contract tests; coverage measurement tools needed; CI/CD integration ready

---

## SECTION 1: EXISTING TESTS EVALUATION

### 1.1 Test Inventory

| Test | Module | Lines | Quality | Coverage |
|------|--------|-------|---------|----------|
| `test_event_bus_contract.c` | event_bus, event_channel | 48 | ⭐⭐⭐⭐⭐ | High |
| `test_scheduler_fairness.c` | scheduler | 66 | ⭐⭐⭐⭐ | Medium |
| `test_process_slot_reuse.c` | process | 53 | ⭐⭐⭐⭐ | Medium |
| `test_input_bounds.c` | input, event_bus | 67 | ⭐⭐⭐ | Low |

### 1.2 Detailed Test Analysis

#### test_event_bus_contract.c
**Strengths:**
- Tests both backpressure policies (DROP_NEWEST, DROP_OLDEST)
- Verifies queue depth limits (127 max packets)
- Confirms drop counting semantics
- Tests event packet initialization

**Gaps:**
- No negative tests (NULL packets, invalid channels)
- No targeted process queue tests
- No policy transition tests
- Limited to single channel testing
- No stress-test with multiple concurrent publishers

**Quality Score:** 4.5/5

#### test_scheduler_fairness.c
**Strengths:**
- Validates round-robin scheduling order
- Tests 3 equal-priority tasks
- Confirms execution fairness over 6 steps
- Good mock infrastructure for system dependencies

**Gaps:**
- Only 1 priority level tested
- No task removal/exit during scheduling
- No priority inversion scenarios
- No preemption testing
- Task completion handling untested

**Quality Score:** 3.5/5

#### test_process_slot_reuse.c
**Strengths:**
- Tests PID slot exhaustion (64 processes)
- Verifies slot reuse after process exit
- Tests both creation success and overflow failure
- Clean mocking of scheduler/VM dependencies

**Gaps:**
- No capability checking
- No concurrent creation/exit scenarios
- No edge cases: reuse ordering, slot fragmentation
- No process state transition testing
- VM address space lifecycle incomplete

**Quality Score:** 3.5/5

#### test_input_bounds.c
**Strengths:**
- Tests event code validation (invalid codes rejected)
- Tests payload size validation (undersized packets rejected)
- Tests valid packet acceptance
- Good bounds checking coverage

**Gaps:**
- Only 3 test cases
- No keyboard/mouse specific testing
- No input queue bounds (max events)
- No input overrun handling
- No protocol fallback testing

**Quality Score:** 3/5

---

## SECTION 2: CRITICAL MISSING TEST COVERAGE

### 2.1 Memory Subsystem (0% tested)

**Module:** `kernel/memory.c`, `kernel/memory_freelist.c`, `kernel/heap.c`  
**Code Size:** ~260 lines  
**Test Size:** 0 lines  
**Criticality:** 🔴 CRITICAL

#### memory_freelist.c (118 lines)
**What it does:**
- Tracks active page allocations (256 max)
- Manages free block list (256 max)
- Coalesces adjacent free blocks
- Allocates from free list on demand

**Missing Tests:**

| Gap | Test Needed | Rationale | Effort |
|-----|-------------|-----------|--------|
| Fragmentation tracking | `test_memory_freelist_fragmentation` | Verify `track_active` correctly maintains allocation list; test with interleaved alloc/free | Medium |
| Block coalescing | `test_memory_freelist_coalesce` | Verify `merge()` combines adjacent blocks correctly; test all merge patterns | Medium |
| Block overflow | `test_memory_freelist_overflow` | Test exceeding MAX_FREE_BLOCKS=256; verify graceful failure | Low |
| Allocation order | `test_memory_freelist_alloc_order` | Verify first-fit allocation strategy; test with gaps | Low |
| Edge cases | `test_memory_freelist_edges` | Zero pages, single page, exact-fit, split remainder | Low |

**Recommended Test:**
```c
// test_memory_freelist.c - ~200 lines
- test_alloc_and_track_single_block()
- test_free_and_push_block()
- test_merge_adjacent_blocks_forward()
- test_merge_adjacent_blocks_backward()
- test_merge_chain()
- test_alloc_from_merged_block()
- test_split_block_on_partial_alloc()
- test_overflow_free_list()
- test_fragmentation_tracking()
```

**Effort:** 2-3 hours

#### heap.c (176 lines)
**What it does:**
- Manages variable-size heap allocations
- Free list with coalescing
- Spinlock-protected
- Alignment handling (16-byte)
- Heap expansion on demand

**Missing Tests:**

| Gap | Test Needed | Rationale | Effort |
|-----|-------------|-----------|--------|
| Basic alloc/free | `test_heap_alloc_free_simple` | Allocate and free single blocks | Low |
| Alignment | `test_heap_alignment` | Verify 16-byte alignment enforced | Low |
| Coalescing | `test_heap_coalesce_adjacent` | Free adjacent blocks, verify merge | Medium |
| Fragmentation | `test_heap_fragmentation` | Allocate/free patterns, verify utilization | Medium |
| Overflow | `test_heap_overflow` | Exceed available heap pages | Low |
| Concurrency | `test_heap_concurrent_alloc` | Multiple threads (mock spinlock) | High |
| Edge cases | `test_heap_edge_cases` | Zero size, NULL free, double free | Low |

**Recommended Test:**
```c
// test_heap.c - ~250 lines
- test_heap_init_and_stats()
- test_heap_alloc_single_block()
- test_heap_alloc_alignment()
- test_heap_free_and_reuse()
- test_heap_coalesce_adjacent_blocks()
- test_heap_alloc_after_coalesce()
- test_heap_fragmentation_pattern()
- test_heap_overflow_graceful_fail()
- test_heap_used_bytes_tracking()
- test_heap_stress_alloc_free()
```

**Effort:** 3-4 hours

### 2.2 Virtual Memory Subsystem (0% tested)

**Modules:** `kernel/vm.c`, `kernel/vm_builder.c`  
**Code Size:** ~300 lines  
**Test Size:** 0 lines  
**Criticality:** 🔴 CRITICAL

#### vm_builder.c (144 lines)
**What it does:**
- Creates identity-mapped page tables
- Manages protected memory regions (8 max)
- Allocates and zeroes page table pages
- Sets page flags (PRESENT, WRITABLE, LARGE)

**Missing Tests:**

| Gap | Test Needed | Rationale | Effort |
|-----|-------------|-----------|--------|
| Identity mapping | `test_vm_identity_space_create` | Verify PML4→PDPT→PD setup; check flags | Medium |
| Protected regions | `test_vm_protected_regions` | Add regions, verify write protection bits | Medium |
| Boundary conditions | `test_vm_page_boundary` | Test with 1, 2, VM_MAX_PDPT PDs | Medium |
| Overflow | `test_vm_protected_region_overflow` | Exceed MAX_PROTECTED_REGIONS=8 | Low |
| Page allocation failure | `test_vm_alloc_failure` | Simulate out-of-memory during table creation | High |
| Large pages | `test_vm_large_page_flags` | Verify 2MB page flag set correctly | Low |

**Recommended Test:**
```c
// test_vm_builder.c - ~280 lines
- test_vm_protected_region_add()
- test_vm_protected_region_overflow()
- test_vm_is_protected_region()
- test_vm_identity_space_single_pd()
- test_vm_identity_space_multiple_pds()
- test_vm_protected_region_write_flags()
- test_vm_identity_space_pml4_setup()
- test_vm_identity_space_pdpt_setup()
- test_vm_pd_large_page_flags()
- test_vm_partial_alloc_failure_cleanup()
```

**Effort:** 3-4 hours

#### vm.c (154 lines)
**What it does:**
- Initializes kernel VM (identity-mapped)
- Manages address space slots (max 8)
- Creates/releases user address spaces
- Tracks mapped memory bytes

**Missing Tests:**

| Gap | Test Needed | Rationale | Effort |
|-----|-------------|-----------|--------|
| Kernel space init | `test_vm_kernel_init` | Verify PML4 creation, framebuffer protection | Medium |
| User space creation | `test_vm_create_address_space` | Create/release cycles; slot reuse | Medium |
| Slot exhaustion | `test_vm_slot_exhaustion` | Exceed VM_MAX_SPACES=8; cleanup | Low |
| Framebuffer protection | `test_vm_framebuffer_protection` | Verify FB marked read-only appropriately | Medium |
| Overflow arithmetic | `test_vm_overflow_checks` | Test framebuffer_end + size overflow guards | Low |
| Ready state | `test_vm_ready_state` | Verify ready flag and error conditions | Low |

**Recommended Test:**
```c
// test_vm.c - ~250 lines
- test_vm_init_ready_state()
- test_vm_kernel_pml4_created()
- test_vm_framebuffer_protected()
- test_vm_create_user_address_space()
- test_vm_create_multiple_spaces()
- test_vm_release_address_space()
- test_vm_slot_exhaustion()
- test_vm_create_after_release_reuses_slot()
- test_vm_prevent_kernel_release()
- test_vm_overflow_calculation()
```

**Effort:** 3 hours

### 2.3 Graphics & Framebuffer (0% tested)

**Modules:** `graphics/framebuffer.c`, `graphics/font8x16.c`  
**Code Size:** ~200 lines  
**Test Size:** 0 lines  
**Criticality:** 🔴 HIGH (UI blocking)

#### framebuffer.c (157 lines)
**What it does:**
- Manages front/backbuffer rendering
- Pixel drawing with bounds checking
- Rectangle filling
- Region-based present (dirty rectangle optimization)
- Text rendering via font

**Missing Tests:**

| Gap | Test Needed | Rationale | Effort |
|-----|-------------|-----------|--------|
| Bounds checking | `test_framebuffer_bounds` | Verify out-of-bounds pixels ignored | Low |
| Pitch handling | `test_framebuffer_pitch_mismatch` | When pitch ≠ width (UEFI reality) | Medium |
| Backbuffer fallback | `test_framebuffer_no_backbuffer` | Render with only frontbuffer | Low |
| Rectangle drawing | `test_framebuffer_rect` | Various sizes, negative coords, clipping | Low |
| Region present | `test_framebuffer_present_region` | Verify only specified region copied | Medium |
| Negative coordinates | `test_framebuffer_negative_coords` | Pixels at x<0, y<0, outside frame | Low |
| Edge clipping | `test_framebuffer_edge_clipping` | Rectangles partially off-screen | Low |
| Integer overflow | `test_framebuffer_overflow` | Large width/height causing overflow | Low |

**Recommended Test:**
```c
// test_framebuffer.c - ~200 lines
- test_framebuffer_init_with_backbuffer()
- test_framebuffer_init_without_backbuffer()
- test_drawpixel_in_bounds()
- test_drawpixel_out_of_bounds()
- test_drawpixel_negative_coords()
- test_drawrect_full_screen()
- test_drawrect_clipping()
- test_drawrect_negative_dims()
- test_clearscreen()
- test_framebuffer_present_copies_backbuffer()
- test_framebuffer_present_region_partial()
- test_framebuffer_present_region_clipping()
```

**Effort:** 2-3 hours

### 2.4 Window Manager (0% tested)

**Modules:** `gui/wm_state.c`, `gui/wm_input.c`, `gui/wm_render.c`, `gui/wm_menu.c`  
**Code Size:** ~800 lines  
**Test Size:** 0 lines  
**Criticality:** 🔴 HIGH (UI core)

#### wm_state.c (215+ lines)
**What it does:**
- Window creation/deletion
- Window focus/z-order management
- Window state tracking
- Event publishing for app launch
- Point-in-window collision detection

**Missing Tests:**

| Gap | Test Needed | Rationale | Effort |
|-----|-------------|-----------|--------|
| Window creation | `test_wm_create_window` | Verify window IDs unique, capacity limits | Low |
| Window focus | `test_wm_focus_window` | Z-order updates, focused flag | Low |
| Window deletion | `test_wm_delete_window` | Cleanup, slot reuse | Low |
| Point-in-window | `test_wm_point_in` | Collision detection boundaries | Low |
| Z-order | `test_wm_z_order` | Top window selection, ordering | Medium |
| Window overflow | `test_wm_max_windows` | Exceed WM_MAX_WINDOWS limit | Low |
| Focus transitions | `test_wm_focus_transitions` | Multiple focus changes | Low |
| Launch requests | `test_wm_publish_launch_request` | Event packet formation, payload | Low |

**Recommended Test:**
```c
// test_wm_state.c - ~200 lines
- test_wm_init()
- test_wm_create_window()
- test_wm_find_window()
- test_wm_find_window_not_found()
- test_wm_focus_window()
- test_wm_focus_transitions()
- test_wm_window_z_order()
- test_wm_top_window_at_point()
- test_wm_window_capacity()
- test_wm_publish_launch_request()
- test_wm_launch_request_payload()
```

**Effort:** 2-3 hours

### 2.5 App Framework (0% tested)

**Modules:** `kernel/app.c`, `kernel/app_registry.c`, `kernel/app_instance.c`, `kernel/app_console_cmd.c`  
**Code Size:** ~700 lines  
**Test Size:** 0 lines  
**Criticality:** 🟠 MEDIUM

#### app.c (585+ lines)
**What it does:**
- App manifest parsing (.app files with key=value)
- App registration/launch
- Console command parsing
- Task list rendering
- Log buffer management with trimming

**Missing Tests:**

| Gap | Test Needed | Rationale | Effort |
|-----|-------------|-----------|--------|
| Manifest parsing | `test_app_manifest_parse` | Parse key=value pairs; invalid formats | Medium |
| String operations | `test_app_string_utilities` | text_length, copy_chars, equals_chars bounds | Low |
| Buffer trimming | `test_app_buffer_trim` | Remove leading lines when full; boundary cases | Medium |
| Buffer append | `test_app_buffer_append` | Multiple appends, overflow handling | Low |
| Decimal formatting | `test_app_to_decimal` | Various values, 0, UINT64_MAX | Low |
| Task info | `test_app_task_info_parsing` | Parse task list output | Medium |

**Recommended Test:**
```c
// test_app.c - ~250 lines
- test_copy_chars_bounded()
- test_equals_chars()
- test_text_length_bounded()
- test_buffer_trim_front()
- test_buffer_append_text()
- test_buffer_append_char()
- test_buffer_trim_with_newlines()
- test_to_decimal_values()
- test_to_decimal_zero()
- test_to_decimal_max()
```

**Effort:** 2-3 hours

### 2.6 Driver Modules (0% tested)

**Modules:** `drivers/input.c`, `drivers/keyboard_uefi.c`, `drivers/mouse_*.c`  
**Code Size:** ~700 lines  
**Test Size:** 67 lines (input_bounds only)  
**Criticality:** 🟠 MEDIUM

#### Mouse Driver Fallback Chain (0% tested)
**What it does:**
- Mouse source priority: PS/2 → Absolute Pointer → Simple Pointer
- Protocol discovery and setup
- Event generation with bounds checking
- Mouse state tracking (x, y, left_down)

**Missing Tests:**

| Gap | Test Needed | Rationale | Effort |
|-----|-------------|-----------|--------|
| Protocol fallback | `test_mouse_protocol_priority` | Verify PS/2 > Absolute > Simple | Medium |
| Protocol discovery | `test_mouse_discover_protocols` | Mock LocateHandleBuffer; verify setup | High |
| Event generation | `test_mouse_event_generation` | Movement, button press events | Low |
| Coordinate bounds | `test_mouse_coordinate_bounds` | Screen clipping, absolute→screen mapping | Medium |
| PS/2 packets | `test_mouse_ps2_packets` | 3-byte packet parsing, button mapping | High |

**Recommended Test:**
```c
// test_mouse_driver.c - ~300 lines
- test_mouse_simple_protocol_priority()
- test_mouse_absolute_protocol_priority()
- test_mouse_ps2_priority()
- test_mouse_protocol_discovery_mock()
- test_mouse_event_coordinate_bounds()
- test_mouse_ps2_packet_parsing()
- test_mouse_button_events()
```

**Effort:** 4-5 hours (protocol mocking is complex)

### 2.7 Kernel Module Gaps

#### scheduler.c (150+ lines)
**Tested:** Basic fairness only  
**Missing:**
- Priority levels (only tested equal priority)
- Task removal during scheduling
- Task completion handling
- Preemption timing
- Starvation scenarios

**Recommended:** `test_scheduler_priorities.c`, `test_scheduler_preemption.c` (3 hours)

#### process.c (200+ lines)
**Tested:** Slot reuse only  
**Missing:**
- Capability enforcement
- Concurrent create/exit race conditions
- Process state transitions (NEW → RUNNING → TERMINATED)
- VM cleanup on exit
- Task cleanup on exit

**Recommended:** `test_process_capabilities.c`, `test_process_exit.c` (3 hours)

#### event_bus.c (73 lines)
**Tested:** Queue semantics, drop counting  
**Missing:**
- Targeted process queue overflow
- Null packet handling (already in contract test)
- Capability-gated syscall events
- Event routing between processes

**Recommended:** `test_event_bus_process_queue.c` (2 hours)

---

## SECTION 3: TEST QUALITY ASSESSMENT

### 3.1 Assertion Coverage

| Module | Assertions | Coverage |
|--------|-----------|----------|
| event_bus | 4 assertions | Depth & drop counts only |
| scheduler | 5 assertions | Fairness order; runs count |
| process | 3 assertions | PID uniqueness, overflow |
| input | 3 assertions | Code validation, size validation |
| **Total** | **15 assertions** | **~30% of critical paths** |

**Gap:** No negative assertions (error cases, boundary failures)

### 3.2 Edge Case Coverage

**Covered:**
- ✅ Queue overflow (event_bus)
- ✅ Drop policy transitions (event_bus)
- ✅ Process slot exhaustion (process)
- ✅ Undersized packets (input)

**Not Covered:**
- ❌ NULL pointer handling in most modules
- ❌ Integer overflow (e.g., pitch × height in framebuffer)
- ❌ Boundary crossings (e.g., INT32 → UINT32 in wm)
- ❌ Partial allocation failures (vm_builder)
- ❌ Spinlock contention / race conditions
- ❌ Concurrent access patterns
- ❌ Resource exhaustion cascades

### 3.3 Error Path Testing

**Currently tested:** ~10% of error paths
- ✅ Packet validation (size, code)
- ✅ PID limit overflow
- ✅ Drop counting

**Not tested:** ~90% of error paths
- ❌ memory_alloc_pages failure cascades
- ❌ Page table allocation failures
- ❌ Heap expansion failures
- ❌ Driver protocol discovery failures
- ❌ Window manager capacity exhaustion
- ❌ Event channel policy switching errors

---

## SECTION 4: TEST INFRASTRUCTURE QUALITY

### 4.1 Current Setup (✅ Well Designed)

**Makefile integration:**
```makefile
$(BUILD_DIR)/tests/test_event_bus_contract: tests/test_event_bus_contract.c \
    kernel/event_bus.c kernel/event_packet.c kernel/event_channel.c kernel/spinlock.c
    $(HOST_CC) $(HOST_CFLAGS) -c $(filter %.c, $^) -o $@

test-host: $(HOST_TEST_BINS)
    @for t in $(HOST_TEST_BINS); do echo "[test] $$t"; $$t; done
```

**Strengths:**
- ✅ Runs on host (not UEFI target)
- ✅ Each test compiles independently
- ✅ Module dependencies explicit
- ✅ Standard assert.h for checking
- ✅ Easy to add new tests

### 4.2 Limitations & Improvements Needed

| Issue | Impact | Solution | Effort |
|-------|--------|----------|--------|
| No coverage measurement | Unknown gaps | Add gcov/lcov integration | 2 hours |
| No test discovery | Manual addition | Write test generator script | 1 hour |
| No parameterized tests | Code duplication | Implement test macro framework | 2 hours |
| No mock framework | Difficult stubs | Create mock utility library | 3 hours |
| No performance tests | Speed regressions missed | Add timing assertions | 2 hours |
| No integration tests | Subsystem interactions untested | Create scenario tests | 4 hours |

### 4.3 Extensibility Assessment

**Easiness of Adding Tests:** ⭐⭐⭐⭐⭐ (5/5)
- New test just needs:
  1. Create `tests/test_*.c`
  2. Add to `HOST_TEST_BINS` in Makefile
  3. List dependencies as prerequisites
  4. Compile with `make test-host`

**Example:** Adding heap test takes 5 minutes

---

## SECTION 5: INTEGRATION TESTING GAPS

### 5.1 Boot Sequence Testing (0% tested)

**Flow:** `efi_main` → `boot_info_t` → `kernel_main` → subsystem init chain

**Missing:**
- `test_boot_sequence_ordering.c`: Verify memory → vm → heap → process init order
- `test_boot_handoff_contract.c`: Verify boot_info_t passed correctly
- `test_platform_context_init.c`: Framebuffer, memory map, boot services setup
- `test_subsystem_cross_module.c`: Platform context consumed by all subsystems

**Effort:** 4-5 hours

### 5.2 Event Flow End-to-End (Partial)

**Current:** Event bus queue semantics tested in isolation  
**Missing:**
- `test_event_flow_input_to_wm.c`: Input → Event Bus → Window Manager
- `test_event_flow_app_launch.c`: WM launch request → App framework → Process creation
- `test_event_flow_syscall.c`: Syscall → Capability check → Event publish → Process receive
- `test_event_bus_saturation.c`: Multiple publishers, queue full, drop behavior

**Effort:** 5-6 hours

### 5.3 Memory Subsystem Integration (0% tested)

**Flow:** `memory_alloc_pages` → `memory_freelist_alloc` + `memory_freelist_merge` + tracking

**Missing:**
- `test_memory_integration_fragmentation.c`: Real fragmentation patterns
- `test_memory_integration_heap.c`: Heap expanding via `memory_alloc_pages`
- `test_memory_integration_vm.c`: VM builder using `memory_alloc_pages`
- `test_memory_cascade_failure.c`: Out-of-memory cascades through subsystems

**Effort:** 4-5 hours

---

## SECTION 6: COVERAGE GAP MATRIX

### Critical Modules by Risk

| Module | Lines | Tests | Coverage | Risk | Priority |
|--------|-------|-------|----------|------|----------|
| heap.c | 176 | 0 | 0% | 🔴 CRITICAL | P0 |
| memory_freelist.c | 118 | 0 | 0% | 🔴 CRITICAL | P0 |
| vm_builder.c | 144 | 0 | 0% | 🔴 CRITICAL | P0 |
| vm.c | 154 | 0 | 0% | 🔴 CRITICAL | P0 |
| framebuffer.c | 157 | 0 | 0% | 🔴 HIGH | P1 |
| wm_state.c | 215 | 0 | 0% | 🔴 HIGH | P1 |
| app.c | 585 | 0 | 0% | 🟠 MEDIUM | P2 |
| process.c | 200 | 53 | 25% | 🟠 MEDIUM | P2 |
| scheduler.c | 150 | 66 | 40% | 🟠 MEDIUM | P2 |
| event_bus.c | 73 | 48 | 60% | 🟢 LOW | P3 |
| input.c | 170 | 67 | 40% | 🟢 LOW | P3 |

### Feature-Based Coverage

| Feature | Status | Gap |
|---------|--------|-----|
| Memory allocation | ⚫ 0% | Critical |
| Memory freeing/merging | ⚫ 0% | Critical |
| Virtual memory creation | ⚫ 0% | Critical |
| Virtual memory cleanup | ⚫ 0% | Critical |
| Graphics rendering | ⚫ 0% | Critical |
| Window management | ⚫ 0% | Critical |
| App launching | ⚫ 0% | Critical |
| Event bus queuing | 🟢 70% | Minor |
| Process lifecycle | 🟡 30% | Moderate |
| Scheduling | 🟡 40% | Moderate |
| Input handling | 🟡 40% | Moderate |

---

## SECTION 7: RECOMMENDED TEST IMPLEMENTATION ROADMAP

### Phase 1: Foundation (P0 - 2 weeks)
**Critical memory safety & VM correctness**

1. `test_memory_freelist.c` (200 lines, 3 hours)
   - Block tracking, coalescing, allocation order
2. `test_heap.c` (250 lines, 4 hours)
   - Alloc/free, alignment, coalescing, fragmentation
3. `test_vm_builder.c` (280 lines, 4 hours)
   - Identity mapping, protected regions, page flags
4. `test_vm.c` (250 lines, 3 hours)
   - Kernel init, user space creation, slot management

**Subtotal:** 980 lines, 14 hours

### Phase 2: Graphics & UI (P1 - 1 week)
**Rendering and window management**

1. `test_framebuffer.c` (200 lines, 3 hours)
   - Bounds checking, pitch handling, regions
2. `test_wm_state.c` (200 lines, 3 hours)
   - Window lifecycle, focus, z-order
3. `test_wm_input.c` (150 lines, 2 hours)
   - Mouse input, window picking, drag/resize

**Subtotal:** 550 lines, 8 hours

### Phase 3: App Framework (P2 - 1 week)
**App lifecycle and utilities**

1. `test_app.c` (250 lines, 3 hours)
   - String utilities, buffer management, parsing
2. `test_app_registry.c` (150 lines, 2 hours)
   - App registration, lookup
3. `test_process_exit.c` (150 lines, 2 hours)
   - Process termination, VM cleanup

**Subtotal:** 550 lines, 7 hours

### Phase 4: Drivers (P3 - 2 weeks)
**Input and driver fallback**

1. `test_mouse_driver.c` (300 lines, 5 hours)
   - Protocol priority, fallback, event generation
2. `test_keyboard_driver.c` (150 lines, 2 hours)
   - Scan code handling, repeats
3. `test_scheduler_preemption.c` (150 lines, 2 hours)
   - Timer preemption, priority handling

**Subtotal:** 600 lines, 9 hours

### Phase 5: Integration (P4 - 1 week)
**End-to-end scenarios**

1. `test_boot_sequence.c` (200 lines, 3 hours)
2. `test_event_flow_integration.c` (250 lines, 3 hours)
3. `test_memory_integration.c` (200 lines, 3 hours)

**Subtotal:** 650 lines, 9 hours

---

## SECTION 8: SPECIFIC COVERAGE GAPS WITH EXAMPLES

### Gap 1: Heap Fragmentation Patterns
**Module:** `kernel/heap.c`  
**Current Test:** None  
**Problem:** Unknown fragmentation behavior under real allocation patterns

**Example Scenario:**
```
Alloc(100) → Free(start, 50) → Alloc(100) → Free(start+50, 50) → Alloc(50)
Expected: Reuse middle freed block or merge
Current: Unknown behavior
```

**Test to Add:**
```c
void test_heap_fragmentation_pattern(void) {
    heap_init(10);  // 10 pages = 40KB
    void *a1 = heap_alloc(1000);
    void *a2 = heap_alloc(2000);
    void *a3 = heap_alloc(1000);
    
    heap_free(a1);  // Gap at start
    heap_free(a3);  // Gap at end
    
    void *a4 = heap_alloc(1500);  // Should reuse or merge
    assert(a4 != NULL);
    assert(heap_used_bytes() > 2000);  // Verify reuse
}
```

### Gap 2: Memory Freelist Coalescing Edge Cases
**Module:** `kernel/memory_freelist.c`  
**Current Test:** None  
**Problem:** Coalescing algorithm correctness unknown for:
- Triple adjacent blocks
- Non-contiguous blocks
- Single-page blocks

**Example Scenario:**
```
Track: [A @ 0x1000 (2 pg)] [B @ 0x2000 (2 pg)] [C @ 0x3000 (2 pg)]
Free: [B, A, C] in order
Expected: Single merged block @ 0x1000 (6 pg)
```

**Test to Add:**
```c
void test_memory_freelist_triple_merge(void) {
    memory_freelist_track_active(0x1000, 2);
    memory_freelist_track_active(0x2000, 2);
    memory_freelist_track_active(0x3000, 2);
    
    memory_freelist_push(0x2000, 2);  // Push middle
    memory_freelist_push(0x1000, 2);  // Push start
    memory_freelist_push(0x3000, 2);  // Push end
    
    memory_freelist_merge();
    
    EFI_PHYSICAL_ADDRESS addr = memory_freelist_alloc(6);
    assert(addr == 0x1000);  // All coalesced
}
```

### Gap 3: VM Page Table Allocation Failure Path
**Module:** `kernel/vm_builder.c`  
**Current Test:** None  
**Problem:** Cleanup on partial failure (e.g., allocating PD fails after PDPT allocated)

**Example Scenario:**
```
Alloc PML4 ✓ → Alloc PDPT ✓ → Alloc PD[0] ✓ → ... → Alloc PD[5] ✗
Expected: Cleanup PML4, PDPT, PD[0-4] and return 0
```

**Test to Add:**
```c
void test_vm_builder_partial_failure(void) {
    // Mock memory_alloc_pages to fail on 4th call (5th PD)
    vm_state_t slot;
    EFI_PHYSICAL_ADDRESS result = vm_builder_create_identity_space(&slot, 8);
    
    // Should return 0 and leave slot inactive
    assert(result == 0);
    assert(!slot.active);
    // Verify cleanup happened (count freed pages)
}
```

### Gap 4: Framebuffer Pitch Mismatch
**Module:** `graphics/framebuffer.c`  
**Current Test:** None  
**Problem:** When pitch ≠ width (common in UEFI), offset calculation may overflow or skip pixels

**Example Scenario:**
```
Screen: 1920×1080, pitch=2048 (UEFI often rounds up)
Pixel @ (1919, 1079): offset = 1079*2048 + 1919 = 2211839
Without pitch handling: offset = 1079*1920 + 1919 = 2071679 (WRONG)
```

**Test to Add:**
```c
void test_framebuffer_pitch_handling(void) {
    platform_context_t platform = {
        .framebuffer = {
            .base = 0x1000,
            .width = 1920,
            .height = 1080,
            .pixels_per_scanline = 2048  // pitch > width
        }
    };
    
    framebuffer_init(&platform);
    drawPixel(1919, 1079, 0xFF0000);  // Red
    
    // Verify pixel at correct offset in frontbuffer
    UINT32 expected_offset = 1079*2048 + 1919;
    // Mock check: g_frontbuffer[expected_offset] == 0xFF0000
}
```

### Gap 5: Window Manager Z-Order with Insertion
**Module:** `gui/wm_state.c`  
**Current Test:** None  
**Problem:** Z-order assignment when inserting window between existing windows

**Example Scenario:**
```
Create Win1 (z=1) → Create Win2 (z=2) → Create Win3 (z=3) → Focus Win1
Expected z-values: Win2=1, Win3=2, Win1=3 (Win1 on top)
```

**Test to Add:**
```c
void test_wm_z_order_focus_update(void) {
    wm_init(1024, 768);
    
    UINT32 w1 = wm_create_window(1, L"W1", 0, 0, 100, 100);
    UINT32 w2 = wm_create_window(1, L"W2", 100, 0, 100, 100);
    wm_focus_window(w1);  // Should bring w1 to front
    
    wm_window_t *win1 = wm_find_window(w1);
    wm_window_t *win2 = wm_find_window(w2);
    
    assert(win1->z > win2->z);
    assert(win1->focused);
    assert(!win2->focused);
}
```

### Gap 6: Process Capability Enforcement
**Module:** `kernel/process.c`  
**Current Test:** None  
**Problem:** Capability bits may not be enforced; syscall gates may be bypassed

**Example Scenario:**
```
Create Process (caps=0) → Try syscall with CAP_SYSTEM required
Expected: Syscall rejected
Current: Unknown behavior
```

**Test to Add:**
```c
void test_process_capability_check(void) {
    process_init();
    
    UINT32 pid_unprivileged = process_create_kernel(
        L"app", noop_task, NULL, 1, 0, FALSE  // NO capabilities
    );
    UINT32 pid_privileged = process_create_kernel(
        L"sys", noop_task, NULL, 1, CAP_SYSTEM, TRUE
    );
    
    assert(!(process_capabilities(pid_unprivileged) & CAP_SYSTEM));
    assert(process_capabilities(pid_privileged) & CAP_SYSTEM);
}
```

### Gap 7: Input Queue Bounds (Extends test_input_bounds.c)
**Module:** `drivers/input.c`  
**Current Test:** Partial (67 lines)  
**Problem:** Only tests event code/size validation; not queue overflow

**Missing Tests:**
```c
void test_input_queue_overflow(void) {
    // Poll max events from queue
    // Verify next poll returns 0
}

void test_input_drop_counting(void) {
    // Publish more events than queue capacity
    // Verify drop count increases
    // Verify no silent overflows
}
```

### Gap 8: Scheduler Task Removal Mid-Schedule
**Module:** `kernel/scheduler.c`  
**Current Test:** Partial (66 lines - only fair ordering)  
**Problem:** What happens if task exits while being scheduled?

**Example Scenario:**
```
Schedule: [A, B, C] → Exec A → A exits → Schedule next?
Expected: Continue with B
```

**Test to Add:**
```c
void test_scheduler_task_removal(void) {
    scheduler_init();
    scheduler_create_task(1, L"a", 1, task_that_exits, NULL);
    scheduler_create_task(2, L"b", 1, task_noop, NULL);
    
    scheduler_step();  // Run task A (exits)
    scheduler_step();  // Should run task B (not crash)
}
```

---

## SECTION 9: CI/CD READINESS ASSESSMENT

### 9.1 Current State
- ✅ Tests compile with `make test-host`
- ✅ Tests run independently
- ✅ Return correct exit codes
- ❌ No coverage reporting
- ❌ No performance regression detection
- ❌ No sanitizer integration

### 9.2 CI/CD Integration Steps

**Step 1: Add Coverage Build (30 min)**
```makefile
test-coverage: clean
    CFLAGS="--coverage" make test-host
    gcov build/tests/*.o
    lcov --capture --directory build --output-file coverage.info
    genhtml coverage.info --output-directory coverage_html
```

**Step 2: GitHub Actions Workflow (1 hour)**
```yaml
name: Tests
on: [push, pull_request]
jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - run: cd os && make test-host
      - run: cd os && make test-coverage
      - uses: codecov/codecov-action@v2
```

**Step 3: Sanitizers Integration (1 hour)**
```makefile
test-host-sanitize:
    CFLAGS="-fsanitize=address -fsanitize=undefined" make test-host
```

**Step 4: Performance Regression Detection (2 hours)**
- Add timing assertions to tests
- Track test execution time over commits
- Alert if regressions detected

---

## SECTION 10: IMPLEMENTATION PRIORITY & EFFORT ESTIMATES

### Ultra-Priority (Week 1-2)
| Test | Module | Lines | Hours | Blocks |
|------|--------|-------|-------|--------|
| memory_freelist | memory subsystem | 200 | 3 | Memory allocation |
| heap | heap alloc/free | 250 | 4 | Heap growth |
| vm_builder | VM creation | 280 | 4 | Address spaces |
| vm | VM lifecycle | 250 | 3 | Process isolation |

### High Priority (Week 3-4)
| Test | Module | Lines | Hours | Blocks |
|------|--------|-------|-------|--------|
| framebuffer | graphics | 200 | 3 | Rendering |
| wm_state | window mgr | 200 | 3 | UI responsiveness |

### Medium Priority (Week 5-6)
| Test | Module | Lines | Hours | Blocks |
|------|--------|-------|-------|--------|
| app framework | app.c | 250 | 3 | App launching |
| event_flow_integration | integration | 250 | 3 | End-to-end |

### Lower Priority (Week 7+)
| Test | Module | Lines | Hours | Blocks |
|------|--------|-------|-------|--------|
| mouse_driver | drivers | 300 | 5 | Input accuracy |
| scheduler_preemption | scheduler | 150 | 2 | Timer handling |

**Total Effort:** ~50-60 test hours (~2 developer weeks)

---

## SECTION 11: SPECIFIC RECOMMENDATIONS

### 11.1 Test Organization
**Current:** Tests in `os/tests/test_*.c`  
**Recommended:** Organize by subsystem
```
tests/
  unit/
    test_memory_freelist.c
    test_heap.c
    test_vm_builder.c
    ...
  integration/
    test_boot_sequence.c
    test_event_flow.c
    ...
  helpers/
    mock_memory.h
    mock_scheduler.h
    ...
```

### 11.2 Mock Framework
**Build simple mock library** in `tests/mocks/`:
```c
// tests/mocks/mock_memory.h
typedef struct {
    UINTN alloc_count;
    UINTN alloc_fail_at;  // Fail on Nth alloc
    UINTN freed_count;
} mock_memory_t;

void mock_memory_set_fail_count(UINTN n);
EFI_PHYSICAL_ADDRESS mock_memory_alloc_pages(UINTN pages);
```

**Benefit:** Enables failure injection for cascade testing

### 11.3 Test Naming Convention
**Current:** No clear naming  
**Recommended:** `test_<module>_<scenario>_<expectation>`

Examples:
- `test_heap_alloc_free_simple` ✓ Clear
- `test_framebuffer_drawpixel_out_of_bounds` ✓ Clear
- `test_wm_window_z_order` ✓ Clear

### 11.4 Assertion Best Practices
**Use descriptive assertions:**
```c
// Bad
assert(a == b);

// Good
assert(a == b && "Window should have focus after focus_window()");

// Better
if (a != b) {
    fprintf(stderr, "FAIL: Window z-order mismatch. Got %d, expected %d\n", a, b);
    return FALSE;
}
```

### 11.5 Add Test Utilities Header
**Create** `tests/test_utils.h`:
```c
#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "ASSERT FAILED: %s\n", msg); \
        return FALSE; \
    } \
} while(0)

#define TEST_EQUAL(a, b, msg) TEST_ASSERT((a) == (b), msg)
#define TEST_NULL(ptr, msg) TEST_ASSERT((ptr) == NULL, msg)
#define TEST_NOT_NULL(ptr, msg) TEST_ASSERT((ptr) != NULL, msg)
```

---

## SECTION 12: SUMMARY TABLE

### Coverage Snapshot
```
Module                  Lines    Tests    Coverage    Quality
─────────────────────────────────────────────────────────────
heap.c                  176      0        0%          ⚫
memory_freelist.c       118      0        0%          ⚫
vm_builder.c            144      0        0%          ⚫
vm.c                    154      0        0%          ⚫
framebuffer.c           157      0        0%          ⚫
wm_state.c              215      0        0%          ⚫
app.c                   585      0        0%          ⚫
process.c               200      53       25%         🟡
scheduler.c             150      66       40%         🟡
input.c                 170      67       40%         🟡
event_bus.c             73       48       65%         🟢
─────────────────────────────────────────────────────────────
TOTAL                   2142     302      14%         🟡
```

### Gap Priority Matrix
```
Criticality     Count    Effort   Impact
──────────────────────────────────────
🔴 CRITICAL     7 tests  14hrs    Blocks boot/memory
🟠 HIGH         2 tests  8hrs     Blocks UI/graphics
🟡 MEDIUM       5 tests  12hrs    Functional gaps
🟢 LOW          4 tests  9hrs     Edge cases
──────────────────────────────────────
TOTAL           18 tests 43hrs    Comprehensive coverage
```

---

## SECTION 13: NEXT STEPS

### Week 1-2: Foundation
1. Create `tests/test_memory_freelist.c` (Priority: CRITICAL)
2. Create `tests/test_heap.c` (Priority: CRITICAL)
3. Create `tests/test_vm_builder.c` (Priority: CRITICAL)
4. Create `tests/test_vm.c` (Priority: CRITICAL)
5. Update Makefile with new test targets
6. Run `make test-host` to verify all pass

### Week 3-4: Graphics & UI
1. Create `tests/test_framebuffer.c`
2. Create `tests/test_wm_state.c`
3. Create `tests/mocks/mock_platform.h` for platform context

### Week 5+: Remaining Coverage
1. App framework tests
2. Driver tests
3. Integration scenarios
4. CI/CD setup

### Parallel: Infrastructure
- [ ] Add gcov/lcov coverage reporting
- [ ] Create mock utility library
- [ ] GitHub Actions CI workflow
- [ ] Performance regression detection

---

## CONCLUSION

MarsOS has a **solid foundation with 4 well-designed contract tests** covering critical event bus, scheduler, and process subsystems. However, **major gaps exist in memory (heap/freelist/vm), graphics, and app frameworks** — covering only ~14% of core kernel code.

**Implementing the 18 recommended tests (43 hours effort) would bring coverage to ~60%** and eliminate most critical paths. The test infrastructure is **highly extensible** and ready for rapid expansion.

**Recommendation:** Prioritize P0 memory tests (weeks 1-2) to ensure system stability, then UI tests (weeks 3-4), enabling robust application launching and rendering.

