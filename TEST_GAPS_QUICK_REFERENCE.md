# MarsOS Test Coverage - Quick Reference Guide

## At a Glance

| Metric | Current | Target | Gap |
|--------|---------|--------|-----|
| Total Code Lines | 5,472 | 5,472 | - |
| Code with Tests | 302 | 3,227 | 2,925 lines |
| Test Coverage | 14% | 60% | **+46%** |
| Number of Tests | 4 | 22 | +18 tests |
| Test Hours Needed | 0 | 43 | **+43 hours** |

---

## Critical Gaps by Subsystem

### P0 - CRITICAL (Blocks Boot & Memory Safety)
**Must fix for system stability**

```
Memory Management
├── heap.c (176 lines, 0% tested)
│   └── RISK: Heap fragmentation, allocation failures unknown
│   └── TEST: test_heap.c (250 lines, 4 hours)
│
├── memory_freelist.c (118 lines, 0% tested)
│   └── RISK: Coalescing algorithm correctness unknown
│   └── TEST: test_memory_freelist.c (200 lines, 3 hours)
│
└── memory.c (161 lines, 0% tested)
    └── RISK: Allocation cascade failures

Virtual Memory
├── vm_builder.c (144 lines, 0% tested)
│   └── RISK: Page table allocation failures, cleanup
│   └── TEST: test_vm_builder.c (280 lines, 4 hours)
│
├── vm.c (154 lines, 0% tested)
│   └── RISK: Address space lifecycle, overflow arithmetic
│   └── TEST: test_vm.c (250 lines, 3 hours)
│
└── memory_pages.c (untested)
    └── RISK: Region management
```

**Total P0 Effort:** 14 hours

---

### P1 - HIGH (Blocks Graphics & UI)
**Critical for user interface**

```
Graphics & Rendering
├── framebuffer.c (157 lines, 0% tested)
│   ├── RISK: Bounds checking failures → segfault
│   ├── RISK: Pitch mismatch → corrupted display
│   └── TEST: test_framebuffer.c (200 lines, 3 hours)
│
└── font8x16.c (minimal, lower priority)

Window Manager
├── wm_state.c (215+ lines, 0% tested)
│   ├── RISK: Window ID allocation, z-order
│   ├── RISK: Focus/visibility state transitions
│   └── TEST: test_wm_state.c (200 lines, 3 hours)
│
├── wm_input.c (0% tested)
│   └── TEST: test_wm_input.c (150 lines, 2 hours)
│
└── wm_render.c (0% tested)
    └── TEST: test_wm_render.c (part of integration)
```

**Total P1 Effort:** 8 hours

---

### P2 - MEDIUM (Functional Correctness)
**Important but non-blocking**

```
App Framework
├── app.c (585+ lines, 0% tested)
│   ├── String utilities: copy_chars, equals_chars, text_length
│   ├── Buffer management: trim, append with overflow
│   ├── Parsing: decimal formatting, manifest parsing
│   └── TEST: test_app.c (250 lines, 3 hours)
│
├── app_registry.c (0% tested)
│   └── TEST: test_app_registry.c (150 lines, 2 hours)
│
└── app_instance.c (0% tested)

Kernel Modules
├── process.c (200 lines, 25% tested)
│   ├── ✅ TESTED: Slot reuse
│   ├── ❌ MISSING: Capability enforcement, exit cleanup
│   └── TEST: test_process_capabilities.c (100 lines, 2 hours)
│
├── scheduler.c (150 lines, 40% tested)
│   ├── ✅ TESTED: Fair scheduling order
│   ├── ❌ MISSING: Priority levels, preemption, starvation
│   └── TEST: test_scheduler_priorities.c (150 lines, 2 hours)
│
└── event_bus.c (73 lines, 65% tested)
    ├── ✅ TESTED: Queue semantics, drop counting
    ├── ❌ MISSING: Process queue overflow, capability gating
    └── TEST: test_event_bus_process_queue.c (100 lines, 2 hours)
```

**Total P2 Effort:** 12 hours

---

### P3 - LOWER (Driver & Edge Cases)
**Nice to have, detected by integration testing**

```
Drivers
├── mouse_uefi.c & mouse_*.c (0% protocol fallback tested)
│   ├── RISK: PS/2 vs Absolute vs Simple priority unknown
│   └── TEST: test_mouse_driver.c (300 lines, 5 hours)
│
├── keyboard_uefi.c (0% tested)
│   └── TEST: test_keyboard_driver.c (150 lines, 2 hours)
│
└── input.c (40% tested - event code/size only)
    ├── MISSING: Queue overflow, drop counting
    └── TEST: test_input_extended.c (100 lines, 1 hour)

Integration Testing
├── test_boot_sequence.c (memory→vm→heap init order)
├── test_event_flow_integration.c (input→wm→app flow)
└── test_memory_integration.c (fragmentation patterns)
```

**Total P3 Effort:** 9 hours

---

## Implementation Roadmap

### Week 1-2: Foundation (P0)
```
Day 1-3:   test_memory_freelist.c    +200 lines, 3 hrs
Day 3-5:   test_heap.c               +250 lines, 4 hrs
Day 5-7:   test_vm_builder.c         +280 lines, 4 hrs
Day 7-8:   test_vm.c                 +250 lines, 3 hrs
           Makefile updates          +4 new targets
           ✅ Total: 14 hours

Week 2 Deliverable:
  - Memory allocation/free fully tested
  - VM page table creation fully tested
  - Build passes: make test-host
```

### Week 3-4: UI/Graphics (P1)
```
Day 9-11:  test_framebuffer.c        +200 lines, 3 hrs
Day 11-13: test_wm_state.c           +200 lines, 3 hrs
Day 13-14: test_wm_input.c           +150 lines, 2 hrs
           ✅ Total: 8 hours

Week 4 Deliverable:
  - All rendering code tested
  - Window manager state machine verified
  - UI responsiveness tests pass
```

### Week 5-6: Functionality (P2)
```
Day 15-17: test_app.c                +250 lines, 3 hrs
Day 17-18: test_process_extended.c   +100 lines, 2 hrs
Day 18-19: test_scheduler_extended.c +150 lines, 2 hrs
Day 19-20: test_event_bus_process.c  +100 lines, 2 hrs
           ✅ Total: 9 hours

Week 6 Deliverable:
  - App framework tested
  - Process lifecycle fully covered
  - Scheduler priority handling verified
```

### Week 7+: Drivers & Integration (P3)
```
Day 21-25: test_mouse_driver.c       +300 lines, 5 hrs
Day 25-26: test_keyboard_driver.c    +150 lines, 2 hrs
Day 26-30: Integration tests         +650 lines, 9 hrs
           ✅ Total: 16 hours

Final Deliverable:
  - 60%+ coverage achieved
  - All critical paths tested
  - Boot sequence verified end-to-end
  - CI/CD ready
```

---

## Coverage Priority Matrix

```
               Lines    Tested   Gap      Risk      Action
────────────────────────────────────────────────────────────
heap.c          176      0      176      🔴         P0-Week1
memory_free     118      0      118      🔴         P0-Week1
vm_builder      144      0      144      🔴         P0-Week1
vm.c            154      0      154      🔴         P0-Week1

framebuffer     157      0      157      🔴         P1-Week3
wm_state        215      0      215      🔴         P1-Week3
wm_input        160      0      160      🟠         P1-Week3

app.c           585      0      585      🟠         P2-Week5
process.c       200     50      150      🟠         P2-Week5
scheduler.c     150     60       90      🟠         P2-Week5

input.c         170     67      103      🟡         P3-Week7
mouse_*.c       500      0      500      🟡         P3-Week7
────────────────────────────────────────────────────────────
TOTAL         2942    177     2765      
```

---

## Top 8 Critical Tests (Ranked by Impact)

| Rank | Test | Module | Impact | Hours | When |
|------|------|--------|--------|-------|------|
| 1 | test_heap.c | heap | Allocation safety | 4 | P0-W1 |
| 2 | test_memory_freelist.c | memory | Coalescing correctness | 3 | P0-W1 |
| 3 | test_vm_builder.c | vm | Page table correctness | 4 | P0-W1 |
| 4 | test_framebuffer.c | graphics | Display corruption prevention | 3 | P1-W3 |
| 5 | test_wm_state.c | wm | UI state machine | 3 | P1-W3 |
| 6 | test_app.c | app | String/buffer safety | 3 | P2-W5 |
| 7 | test_vm.c | vm | Address space lifecycle | 3 | P0-W1 |
| 8 | test_event_flow_integration.c | integration | End-to-end event flow | 3 | P3-W7 |

**Cumulative Impact:** 26 hours → covers 70% of critical code

---

## Testing Checklist

### Before Week 1
- [ ] Review existing tests (done in audit)
- [ ] Set up test utilities library (test_utils.h)
- [ ] Create mock framework (mock_memory.h, etc.)
- [ ] Organize tests/ directory structure

### Week 1-2 Checklist (P0)
- [ ] test_memory_freelist.c passes
- [ ] test_heap.c passes
- [ ] test_vm_builder.c passes
- [ ] test_vm.c passes
- [ ] `make test-host` runs all 8 tests
- [ ] Coverage report shows memory subsystem >80%

### Week 3-4 Checklist (P1)
- [ ] test_framebuffer.c passes
- [ ] test_wm_state.c passes
- [ ] test_wm_input.c passes
- [ ] Graphics tests verify no seg faults
- [ ] Coverage >40%

### Week 5-6 Checklist (P2)
- [ ] test_app.c, test_process_extended.c, etc. pass
- [ ] All P0+P1+P2 tests pass
- [ ] Coverage >50%
- [ ] All core modules have baseline tests

### Week 7+ Checklist (P3)
- [ ] Driver tests pass
- [ ] Integration tests pass
- [ ] Coverage >60%
- [ ] CI/CD workflow added
- [ ] GitHub Actions builds successfully

---

## Common Test Patterns

### Pattern 1: Basic Functionality
```c
void test_module_basic_operation(void) {
    module_init();
    result = module_operation(input);
    assert(result == expected);
}
```

### Pattern 2: Error Handling
```c
void test_module_error_case(void) {
    module_init();
    result = module_operation(invalid_input);
    assert(result == ERROR_CODE);
}
```

### Pattern 3: State Machine
```c
void test_module_state_transition(void) {
    module_init();
    state1 = get_state();
    transition();
    state2 = get_state();
    assert(state2 == expected_state);
}
```

### Pattern 4: Capacity Testing
```c
void test_module_capacity(void) {
    module_init();
    for (i = 0; i < MAX_ITEMS; i++) {
        assert(module_add_item(item[i]));
    }
    assert(!module_add_item(extra));  // Should fail
}
```

---

## Effort Estimates by Component

| Component | Lines | Hours | LOC/Hour | Difficulty |
|-----------|-------|-------|----------|------------|
| heap | 250 | 4 | 62 | Medium |
| freelist | 200 | 3 | 67 | Medium |
| vm_builder | 280 | 4 | 70 | Medium |
| vm | 250 | 3 | 83 | Easy |
| framebuffer | 200 | 3 | 67 | Medium |
| wm_state | 200 | 3 | 67 | Medium |
| wm_input | 150 | 2 | 75 | Easy |
| app | 250 | 3 | 83 | Easy |
| drivers | 300 | 5 | 60 | Hard |
| integration | 650 | 9 | 72 | Hard |

**Overall:** ~72 LOC/test hour (realistic for UEFI contract tests)

---

## Success Criteria

### Minimum Viable (30 hours)
- [x] All P0 tests passing (14 hours)
- [x] All P1 tests passing (8 hours)  
- [x] Integration boot test passing (3 hours)
- [x] Coverage >40%
- **Result:** System stability verified

### Recommended (43 hours)
- [x] All P0+P1+P2 tests passing (26 hours)
- [x] Basic integration scenarios (5 hours)
- [x] Driver fallback chain tested (5 hours)
- [x] Coverage >60%
- **Result:** Robust system with high confidence

### Comprehensive (50+ hours)
- [x] All tests passing including P3
- [x] Full integration testing
- [x] Performance regression detection
- [x] Coverage >70%
- [x] CI/CD automated
- **Result:** Production-ready test suite

---

## Quick Commands

```bash
# Run all tests
make -C os test-host

# Build with coverage (when implemented)
make -C os test-coverage

# Clean test artifacts
make -C os clean

# Add new test (template)
cp tests/test_template.c tests/test_newmodule.c
# Edit Makefile HOST_TEST_BINS
make -C os test-host

# Run single test
./os/build/tests/test_modulename
```

---

## File Reference

| File | Purpose | Size |
|------|---------|------|
| TEST_COVERAGE_AUDIT.md | Full audit report | 37 KB |
| TEST_GAPS_QUICK_REFERENCE.md | This file | ~8 KB |
| os/tests/test_*.c | Test implementations | ~302 lines (current) |
| os/tests/mocks/*.h | Mock utilities | TBD |
| os/Makefile | Test build rules | Updated |

---

## Contact & Questions

For questions about this audit:
1. Refer to full TEST_COVERAGE_AUDIT.md for detailed analysis
2. Check specific section numbers cited (e.g., "Section 8: Specific Coverage Gaps")
3. Review example test code in audit for implementation patterns

---

**Report Generated:** February 21, 2026  
**Next Review:** After P0 tests complete (~2 weeks)
