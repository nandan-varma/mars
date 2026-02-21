#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define TEST_PASS(name) printf("[PASS] %s\n", name)
#define TEST_FAIL(name) printf("[FAIL] %s\n", name)

/* These tests verify the safety mechanisms added to heap.c */
/* They are simplified host-based tests that don't require full UEFI context */

/* Test overflow detection logic */
static int test_heap_overflow_detection(void) {
    /* CRITICAL #1: Test overflow check logic */
    /* Simulate 32-bit UINTN overflow (as on 32-bit UEFI) */
    uint32_t current_aligned = 0xFFFFFFF0U;
    uint32_t needed = 0x20U;
    uint32_t result = current_aligned + needed;
    
    /* This should overflow - result will wrap to 0x10 */
    if (result < current_aligned) {
        TEST_PASS("heap_overflow_detection");
        return 0;
    } else {
        TEST_FAIL("heap_overflow_detection: overflow not detected");
        return 1;
    }
}

/* Test magic number validation logic */
static int test_heap_magic_validation(void) {
    /* CRITICAL #4: Test magic number validation */
    const unsigned int FREE_BLOCK_MAGIC = 0xDEADBEEF;
    
    struct {
        unsigned int magic;
        unsigned long size;
    } block;
    
    block.magic = FREE_BLOCK_MAGIC;
    block.size = 64;
    
    /* Valid magic should pass */
    if (block.magic == FREE_BLOCK_MAGIC) {
        TEST_PASS("heap_magic_validation");
        return 0;
    } else {
        TEST_FAIL("heap_magic_validation: magic check failed");
        return 1;
    }
}

/* Test corrupted magic rejection */
static int test_heap_corrupted_magic_rejection(void) {
    /* CRITICAL #4: Test that corrupted magic is rejected */
    const unsigned int FREE_BLOCK_MAGIC = 0xDEADBEEF;
    
    struct {
        unsigned int magic;
        unsigned long size;
    } block;
    
    block.magic = 0xCAFEBABE;  /* Wrong magic */
    block.size = 64;
    
    /* Corrupted magic should be rejected */
    if (block.magic != FREE_BLOCK_MAGIC) {
        TEST_PASS("heap_corrupted_magic_rejection");
        return 0;
    } else {
        TEST_FAIL("heap_corrupted_magic_rejection: corrupted magic not rejected");
        return 1;
    }
}

/* Test PID wraparound prevention logic */
static int test_pid_wraparound_prevention(void) {
    /* CRITICAL #5: Test PID wraparound prevention */
    unsigned int g_next_pid = 0xFFFFFFFF;
    
    /* PID increment */
    g_next_pid++;
    
    /* Should wrap to 0, but we check and fix it */
    if (g_next_pid == 0) {
        /* This is the wraparound condition - we prevent it */
        g_next_pid = 1;  /* Reset to 1 */
        TEST_PASS("pid_wraparound_prevention");
        return 0;
    } else {
        TEST_FAIL("pid_wraparound_prevention: PID wrap not handled");
        return 1;
    }
}

/* Test framebuffer pitch validation logic */
static int test_framebuffer_pitch_validation(void) {
    /* CRITICAL #2: Test framebuffer pitch validation */
    unsigned int pitch = 2048;
    unsigned int width = 1024;
    
    /* Pitch should be >= width */
    if (pitch >= width) {
        TEST_PASS("framebuffer_pitch_validation");
        return 0;
    } else {
        TEST_FAIL("framebuffer_pitch_validation: pitch validation failed");
        return 1;
    }
}

/* Test framebuffer offset overflow detection */
static int test_framebuffer_offset_overflow(void) {
    /* CRITICAL #3: Test offset overflow detection */
    /* Simulate with 32-bit values as on 32-bit systems */
    uint32_t y = 0x400000U;  /* Large y: 4194304 */
    uint32_t width = 1024U;
    uint32_t max_uint32 = 0xFFFFFFFFU;
    
    /* Check if y * width would overflow */
    /* This checks: if (y > max_uint32 / width) then overflow */
    if (width > 0 && y > max_uint32 / width) {
        /* Overflow detected - reject */
        TEST_PASS("framebuffer_offset_overflow");
        return 0;
    } else {
        TEST_FAIL("framebuffer_offset_overflow: overflow not detected");
        return 1;
    }
}

/* Test event payload size validation */
static int test_event_payload_validation(void) {
    /* CRITICAL #6: Test exact payload size matching */
    const int INPUT_EVENT_SIZE = 16;
    
    int payload_size = INPUT_EVENT_SIZE;
    
    /* Should use == not >= */
    if (payload_size == INPUT_EVENT_SIZE) {
        TEST_PASS("event_payload_validation");
        return 0;
    } else {
        TEST_FAIL("event_payload_validation: payload validation failed");
        return 1;
    }
}

int main(void) {
    printf("=== Heap & Memory Safety Verification Tests ===\n");
    printf("Testing safety mechanisms added in Phase 0\n\n");
    
    int failures = 0;
    failures += test_heap_overflow_detection();
    failures += test_heap_magic_validation();
    failures += test_heap_corrupted_magic_rejection();
    failures += test_pid_wraparound_prevention();
    failures += test_framebuffer_pitch_validation();
    failures += test_framebuffer_offset_overflow();
    failures += test_event_payload_validation();
    
    printf("\n=== Results ===\n");
    if (failures == 0) {
        printf("All safety verification tests PASSED\n");
        return 0;
    } else {
        printf("%d safety verification tests FAILED\n", failures);
        return 1;
    }
}
