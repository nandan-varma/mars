# Phase 6: Testing & Hardening - Comprehensive Plan

**Objective:** Build enterprise-grade test coverage for MarsOS kernel to ensure security, stability, and performance reliability across all phases (2-5).

**Target:** 95%+ code coverage in critical modules, no known vulnerabilities, performance regression detection.

---

## Strategy Overview

### Goals
1. **Security Hardening:** Fuzz test all input paths (keyboard, mouse, events)
2. **Stability Testing:** Stress test concurrent process creation/destruction
3. **Performance Regression Detection:** Baseline Phase 5 optimizations, detect slowdowns
4. **Memory Safety Expansion:** Add missing edge cases to heap/VM tests
5. **Integration Testing:** Verify phase interactions (security fixes + optimizations + tests)

### Test Organization
- **Host-side tests** (make test-host): Fast validation, no hardware
- **Target-side tests** (optional): Real hardware validation
- **Fuzz tests** (corpus-driven): Automated input generation

---

## Phase 6 Test Modules

### 1. INPUT DRIVER SECURITY TESTS

**Module:** `os/tests/test_input_security_fuzz.c`

**Purpose:** Fuzz keyboard/mouse input for buffer overflows, out-of-bounds reads

**Test Cases:**

#### 1.1 Keyboard Input Fuzz
- Invalid key codes (0xFF, 0x100+, negative)
- Rapid key events (100/sec burst)
- Stuck keys (repeated press without release)
- Unknown key types
- Payload size boundary checks

```c
void test_keyboard_invalid_codes() {
    // Valid key codes: 1-256
    // Invalid: 0, 257+, floating point corruption
    for (UINT8 code = 250; code < 260; code++) {
        input_event_t evt = { INPUT_EVENT_KEY_DOWN, code, 0, 0 };
        ASSERT(!input_is_valid(&evt), "Should reject code %u", code);
    }
}
```

#### 1.2 Mouse Input Fuzz
- Invalid button states (all bits set)
- Coordinate out-of-bounds (< 0, > screen_w/h)
- Floating point movement (fractional pixels)
- PS/2 packet corruption (bit flips)
- Absolute pointer overflow

```c
void test_mouse_ps2_packet_fuzzing() {
    // PS/2 packet: [buttons, dx, dy]
    // Fuzz each byte independently
    UINT8 packet[3] = { 0xFF, 0xFF, 0xFF };  // All bits set
    // Should handle gracefully without crash/buffer overflow
    poll_ps2_packet(packet);  // Must not crash
}
```

#### 1.3 Event Bus Input Validation
- Invalid channel IDs
- Payload size > EVENT_PAYLOAD_BYTES
- Null source/target PIDs
- Conflicting flags

### 2. PROCESS CONCURRENCY STRESS TESTS

**Module:** `os/tests/test_process_stress.c`

**Purpose:** Verify process lifecycle under concurrent load

**Test Cases:**

#### 2.1 Rapid Process Creation/Destruction
- Create 64 processes simultaneously
- Randomly exit processes while creating new ones
- Verify no PID reuse collisions
- Verify no resource leaks (memory, handles)

```c
void test_process_creation_storm() {
    // Phase 2 fixed PID reuse (commit 7da5741)
    // Verify under concurrent load
    for (int i = 0; i < 10; i++) {  // 10 cycles
        process_t procs[64];
        for (int j = 0; j < 64; j++) {
            ASSERT(process_create(&procs[j], "stress_%d", j), 
                   "Create process %d in cycle %d", j, i);
        }
        for (int j = 0; j < 64; j++) {
            // Exit in random order
            process_exit(procs[random() % 64], 0);
        }
    }
}
```

#### 2.2 Event Queue Under Concurrent Load
- Create 10 processes with 100 events/sec each = 1000 events/sec total
- Verify no events dropped or duplicated
- Check event_bus drop counts stay reasonable
- Verify targeted events reach correct process

#### 2.3 Scheduler Fairness Under Load
- Run 32 tasks, each spinning 1000x
- Verify each task gets ~equal CPU time (±10%)
- Verify no task starvation (each completes)
- Bitmap optimization (Phase 5) shouldn't affect fairness

```c
void test_scheduler_fairness_under_load() {
    // Phase 5 added scheduler bitmap
    // Verify fairness not degraded
    task_t tasks[32];
    UINT32 ticks[32] = {0};
    
    // Run each task, count ticks
    for (int i = 0; i < 1000; i++) {
        scheduler_step();
    }
    
    // Verify distribution within ±10%
    UINT32 avg_ticks = total_ticks / 32;
    for (int i = 0; i < 32; i++) {
        ASSERT(ticks[i] >= avg_ticks * 0.9, "Task %d starved", i);
        ASSERT(ticks[i] <= avg_ticks * 1.1, "Task %d over-scheduled", i);
    }
}
```

### 3. PERFORMANCE REGRESSION BASELINE

**Module:** `os/tests/test_performance_baseline.c`

**Purpose:** Baseline Phase 5 optimizations, detect future slowdowns

**Baseline Metrics:**

#### 3.1 Scheduler Performance
- Time 100,000 scheduler_step() calls (no tasks)
  - Baseline (Phase 5 bitmap): <1ms (bitmap empty early-exit)
  - Regression threshold: >2ms (2x baseline)
- Time 100,000 step() calls (64 tasks)
  - Baseline: ~10ms (bitmap fast scanning)
  - Regression threshold: >20ms

#### 3.2 Event Bus Throughput
- Publish 10,000 events to 10 processes
  - Baseline (Phase 5 optimizations): ~5ms
  - Regression threshold: >10ms
- Targeted publish to random process (100,000 events)
  - Baseline (early-exit optimization): ~50ms
  - Regression threshold: >100ms

#### 3.3 Input Event Processing
- Poll keyboard 100,000 times
  - Baseline: <10ms
- Poll mouse (PS/2) 100,000 times
  - Baseline: <20ms (with Phase 5 enabled guard)

```c
typedef struct {
    const char *name;
    UINT64 baseline_cycles;
    UINT64 regression_threshold;
} perf_test_t;

void test_performance_baseline() {
    perf_test_t tests[] = {
        { "scheduler_empty_100k", 10000, 20000 },      // baseline in CPU cycles
        { "event_publish_10k", 50000, 100000 },
        { "keyboard_poll_100k", 100000, 200000 },
        { NULL, 0, 0 }
    };
    
    for (int i = 0; tests[i].name; i++) {
        UINT64 start = timer_ticks();
        run_test(tests[i].name);
        UINT64 elapsed = timer_ticks() - start;
        
        ASSERT(elapsed <= tests[i].regression_threshold,
               "%s exceeded threshold: %lu > %lu",
               tests[i].name, elapsed, tests[i].regression_threshold);
    }
}
```

### 4. MEMORY SAFETY EXPANSION TESTS

**Module:** `os/tests/test_memory_safety_expanded.c`

**Purpose:** Fill gaps in memory safety coverage (Phase 4)

**New Test Cases:**

#### 4.1 Heap Edge Cases
- Allocate max size (near HEAP_SIZE)
- Free in random order (10-50 allocations)
- Detect fragmentation threshold
- Allocation after heavy fragmentation
- Allocate then immediately free (buddy coalescing)

#### 4.2 Freelist Saturation
- Fill all MAX_FREE_BLOCKS slots (256)
- Attempt to free one more (should succeed after coalesce)
- Verify no data corruption
- Verify coalescing worked

#### 4.3 VM Safety Under Pressure
- Create 10 address spaces
- Map 1000 regions each (up to MAX_REGIONS limit)
- Verify isolation: access in wrong space fails
- Unmap in random order, verify clean unmap

#### 4.4 Event Packet Payload Safety
- Payloads of all sizes: 1, 8, 64 bytes
- Word-aligned optimization (Phase 5) shouldn't affect safety
- Verify no off-by-one in qword copying
- Verify zero-fill of remainder

```c
void test_event_packet_payload_safety() {
    event_packet_t pkt = {0};
    UINT8 payload[64];
    
    // Test various sizes with Phase 5 optimization
    for (UINTN size = 1; size <= 64; size++) {
        event_packet_copy_payload(pkt.payload, payload, size, 64);
        
        // Verify exactly 'size' bytes copied
        ASSERT(pkt.payload[size-1] == payload[size-1], "Size %u copy failed", size);
        if (size < 64) {
            ASSERT(pkt.payload[size] == 0, "Size %u zero-fill failed", size);
        }
    }
}
```

### 5. SECURITY AUDIT VERIFICATION TESTS

**Module:** `os/tests/test_security_audit_fixes.c`

**Purpose:** Verify all Phase 2-3 security fixes remain intact

**Coverage by CWE:**

#### 5.1 CWE-416: Use-After-Free
- Create process, exit it, try to use capabilities (Phase 3 HIGH #5)
- Should fail gracefully, not crash
- Spinlock protection verified

#### 5.2 CWE-835: Infinite Loop  
- Trigger PS/2 polling timeout (Phase 3 HIGH #8)
- Verify loop exits after 1000 attempts, not 100,000
- No system hang

#### 5.3 CWE-59: TOCTOU (Time-of-Check-Time-of-Use)
- Check-Then-Act race in process scheduling (Phase 2)
- Rapid process exit while scheduler examining it
- Should not cause crash/memory corruption

#### 5.4 CWE-657: Improper Event Handling
- Send 1000 invalid event types to WM (Phase 3 HIGH #6)
- Verify validation prevents crashes
- No NULL dereference

---

## Test Architecture

### Build Integration
```makefile
# Add to os/Makefile
test-host: test_input_security_fuzz test_process_stress \
           test_performance_baseline test_memory_safety_expanded \
           test_security_audit_fixes
```

### Test Framework
- Use existing test patterns (see test_heap.c)
- Minimal assertions: focus on crashes/memory safety
- No printf spam; only failures reported

### Performance Test Integration
- Baseline tests run last (don't affect earlier tests)
- Cache warm-up: 1000 "dry runs" before measuring
- Measure in CPU cycles (timer_ticks()), not wall clock

---

## Execution Plan

### Week 1: Core Tests
1. Input security fuzz tests (2 days)
2. Process concurrency stress tests (2 days)
3. Build integration, basic runs (1 day)

### Week 2: Performance & Validation
1. Performance baseline tests (2 days)
2. Memory safety expansion (2 days)
3. Security audit verification (1 day)

### Week 3: Reporting & Hardening
1. Run full test suite on QEMU/real hardware
2. Fix any regressions or failures
3. Generate coverage report
4. Document findings

---

## Success Criteria

### Quantitative
- ✅ 95%+ code coverage in critical modules:
  - scheduler.c, process.c (scheduling/lifecycle)
  - event_bus.c, event_channel.c (event system)
  - input.c, mouse_uefi.c (input drivers)
  - memory.c, heap.c (memory management)
- ✅ 0 crashes under fuzz (10,000+ random inputs)
- ✅ 0 memory leaks under stress (1000+ process cycles)
- ✅ Performance baselines established (no regression >2x)

### Qualitative
- ✅ All Phase 2-5 security/performance fixes verified
- ✅ Edge cases documented
- ✅ Regression detection automated
- ✅ Test suite maintainable for future phases

---

## Phase 6 Deliverables

1. **test_input_security_fuzz.c** (~200 lines)
   - Keyboard/mouse/event validation fuzz
   
2. **test_process_stress.c** (~300 lines)
   - Concurrent process creation/destruction
   - Event queue under load
   
3. **test_performance_baseline.c** (~200 lines)
   - Scheduler, event bus, input throughput
   - Regression detection thresholds
   
4. **test_memory_safety_expanded.c** (~200 lines)
   - Heap saturation, fragmentation
   - VM isolation, freelist coalescing
   
5. **test_security_audit_fixes.c** (~150 lines)
   - CWE-416, CWE-835, CWE-59, CWE-657 verification
   
6. **PHASE_6_TEST_REPORT.md**
   - Coverage summary
   - Test results
   - Recommendations for Phase 7

**Total:** ~1150 lines of test code + documentation

---

## Notes

### Ordering Rationale
1. **Input security first** - Most likely vector for exploits
2. **Process stress second** - Validate scheduling fixes under load
3. **Performance third** - Baseline optimizations early
4. **Memory safety fourth** - Verify allocator edge cases
5. **Security audit last** - Verify all previous phases' fixes

### Resource Efficiency
- Tests use existing UEFI/kernel APIs
- No hardware dependencies for host tests
- Fast execution (<1 second per test module)
- Can run in CI/CD pipeline

### Future Work (Phase 7+)
- Interrupt-driven I/O testing (vs polling)
- NUMA memory allocation testing (if multi-socket)
- Power management integration
- Network stack testing (if added)

