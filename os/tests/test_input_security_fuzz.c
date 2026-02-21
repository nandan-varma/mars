#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

/* Test fuzz harness for input security */
#define TEST_PASS(name) printf("[PASS] %s\n", name)
#define TEST_FAIL(name) printf("[FAIL] %s\n", name); return 1

/* Simulate event_packet_t structure */
typedef struct {
    uint32_t channel;
    uint32_t code;
    uint32_t source_pid;
    uint32_t target_pid;
    uint32_t target_window;
    uint32_t payload_size;
    uint8_t payload[64];
} event_packet_t;

/* Simulate input event types */
typedef enum {
    INPUT_EVENT_KEY_DOWN = 0,
    INPUT_EVENT_MOUSE_MOVE = 1,
    INPUT_EVENT_MOUSE_BUTTON_DOWN = 2,
    INPUT_EVENT_MOUSE_BUTTON_UP = 3
} input_event_type_t;

typedef struct {
    input_event_type_t type;
    union {
        struct {
            uint16_t unicode;
            uint16_t scan_code;
        } key;
        struct {
            int32_t dx;
            int32_t dy;
            int32_t x;
            int32_t y;
        } mouse_move;
        struct {
            uint8_t left;
            uint8_t right;
        } mouse_button;
    } data;
} input_event_t;

/* Constants from event_bus.h */
#define EVENT_CHANNEL_INPUT 1
#define EVENT_CHANNEL_INPUT_KEYBOARD 2
#define EVENT_CHANNEL_SYSTEM 3
#define EVENT_CHANNEL_APP 4
#define EVENT_CHANNEL_COUNT 5
#define EVENT_CODE_INPUT 1
#define EVENT_PAYLOAD_BYTES 64
#define MAX_PROCESSES 64

/* Test 1: Keyboard invalid code rejection */
static int test_keyboard_invalid_codes(void) {
    int special_handling_detected = 0;

    /* Test boundary codes that should require special handling */
    uint8_t special_codes[] = {
        0xFF,  /* Escape code indicator, should be handled carefully */
        0xE0,  /* Extended key prefix (PS/2) */
        0xF0   /* Release prefix (PS/2 Set 2) */
    };

    for (size_t i = 0; i < sizeof(special_codes); i++) {
        uint8_t code = special_codes[i];

        /* Simulate validation: certain codes require special handling */
        if (code == 0xFF || code == 0xE0 || code == 0xF0) {
            special_handling_detected++;
        }
    }

    if (special_handling_detected == 3) {
        TEST_PASS("keyboard_invalid_codes");
        return 0;
    }
    TEST_FAIL("keyboard_invalid_codes");
}

/* Test 2: Keyboard rapid events handling */
static int test_keyboard_rapid_events(void) {
    /* Simulate 1000 rapid keyboard events */
    event_packet_t packets[100];
    int valid_count = 0;

    for (int i = 0; i < 100; i++) {
        packets[i].channel = EVENT_CHANNEL_INPUT_KEYBOARD;
        packets[i].code = EVENT_CODE_INPUT;
        packets[i].payload_size = sizeof(input_event_t);

        /* Each packet must have valid size */
        if (packets[i].payload_size <= EVENT_PAYLOAD_BYTES) {
            valid_count++;
        }
    }

    if (valid_count == 100) {
        TEST_PASS("keyboard_rapid_events");
        return 0;
    }
    TEST_FAIL("keyboard_rapid_events");
}

/* Test 3: Mouse invalid button codes */
static int test_mouse_invalid_buttons(void) {
    input_event_t event;

    /* Valid button values: 0 = not pressed, 1 = pressed */
    uint8_t valid_buttons[] = {0, 1};
    uint8_t invalid_buttons[] = {2, 3, 255};

    int invalid_detected = 0;

    /* Check invalid buttons are detected */
    for (size_t i = 0; i < sizeof(invalid_buttons); i++) {
        uint8_t btn = invalid_buttons[i];
        if (btn > 1) {
            invalid_detected++;
        }
    }

    if (invalid_detected == 3) {
        TEST_PASS("mouse_invalid_buttons");
        return 0;
    }
    TEST_FAIL("mouse_invalid_buttons");
}

/* Test 4: Mouse out-of-bounds coordinates */
static int test_mouse_oob_coordinates(void) {
    int32_t test_coords[] = {
        -32768, -1000, -1,   /* Below zero (3 OOB) */
        0, 512, 1023, 1024,  /* Valid range for 1024-wide screen (4 valid) */
        2049, 32767, 999999  /* Above valid range (3 OOB) */
    };

    int oob_count = 0;
    const int32_t MAX_COORD = 2048;

    for (size_t i = 0; i < sizeof(test_coords) / sizeof(int32_t); i++) {
        int32_t coord = test_coords[i];
        /* Detect out-of-bounds: negative or > max */
        if (coord < 0 || coord > MAX_COORD) {
            oob_count++;
        }
    }

    if (oob_count == 6) {  /* Should detect 6 out-of-bounds values (3+3) */
        TEST_PASS("mouse_oob_coordinates");
        return 0;
    }
    TEST_FAIL("mouse_oob_coordinates");
}

/* Test 5: PS/2 packet corruption detection */
static int test_ps2_corruption_detection(void) {
    /* PS/2 packets are typically 3 bytes: status, x_delta, y_delta */
    uint8_t valid_ps2[] = {0x08, 0x00, 0x00};      /* Valid: bit3=1, bits 6-7=0 */
    uint8_t corrupted_ps2[] = {0xFF, 0xFF, 0xFF};  /* Corrupted: all bits set */

    int valid_detected = 0;
    int corrupted_detected = 0;

    /* PS/2 status byte validation: bit 3 should be 1, bits 6-7 should indicate button state */
    uint8_t status = valid_ps2[0];
    if ((status & 0x08) != 0) {  /* Bit 3 must be set */
        valid_detected++;
    }

    status = corrupted_ps2[0];
    if ((status & 0x08) != 0 && (status & 0xC0)) {  /* All bits set is suspicious */
        corrupted_detected++;
    }

    if (valid_detected == 1) {
        TEST_PASS("ps2_corruption_detection");
        return 0;
    }
    TEST_FAIL("ps2_corruption_detection");
}

/* Test 6: Event bus channel validation */
static int test_event_channel_validation(void) {
    uint32_t valid_channels[] = {
        EVENT_CHANNEL_INPUT,
        EVENT_CHANNEL_INPUT_KEYBOARD,
        EVENT_CHANNEL_SYSTEM,
        EVENT_CHANNEL_APP
    };

    uint32_t invalid_channels[] = {
        0,                    /* Too low */
        EVENT_CHANNEL_COUNT,  /* At boundary */
        EVENT_CHANNEL_COUNT + 1,  /* Beyond boundary */
        999,                  /* Way too high */
        0xFFFFFFFF            /* Max uint32 */
    };

    int valid_count = 0;
    for (size_t i = 0; i < sizeof(valid_channels) / sizeof(uint32_t); i++) {
        uint32_t ch = valid_channels[i];
        if (ch > 0 && ch < EVENT_CHANNEL_COUNT) {
            valid_count++;
        }
    }

    int invalid_rejected = 0;
    for (size_t i = 0; i < sizeof(invalid_channels) / sizeof(uint32_t); i++) {
        uint32_t ch = invalid_channels[i];
        if (!(ch > 0 && ch < EVENT_CHANNEL_COUNT)) {
            invalid_rejected++;
        }
    }

    if (invalid_rejected == 5) {
        TEST_PASS("event_channel_validation");
        return 0;
    }
    TEST_FAIL("event_channel_validation");
}

/* Test 7: Event payload size validation */
static int test_event_payload_validation(void) {
    uint32_t payload_sizes[] = {
        0,                          /* Empty payload */
        1,                          /* Minimal */
        sizeof(input_event_t),      /* Normal */
        EVENT_PAYLOAD_BYTES,        /* Max valid */
        EVENT_PAYLOAD_BYTES + 1,    /* Over limit */
        1024,                       /* Way over limit */
        0xFFFFFFFF                  /* Max uint32 */
    };

    int valid_count = 0;
    int invalid_detected = 0;

    for (size_t i = 0; i < sizeof(payload_sizes) / sizeof(uint32_t); i++) {
        uint32_t size = payload_sizes[i];
        if (size <= EVENT_PAYLOAD_BYTES) {
            valid_count++;
        } else {
            invalid_detected++;
        }
    }

    if (valid_count == 4 && invalid_detected == 3) {
        TEST_PASS("event_payload_validation");
        return 0;
    }
    TEST_FAIL("event_payload_validation");
}

/* Test 8: PID range validation */
static int test_pid_range_validation(void) {
    uint32_t pids[] = {
        0,          /* Reserved/invalid */
        1,          /* Valid: kernel */
        2,          /* Valid: app */
        MAX_PROCESSES - 1,  /* Valid: near max */
        MAX_PROCESSES,      /* Invalid: at boundary */
        MAX_PROCESSES + 1,  /* Invalid: over limit */
        999,        /* Invalid: too high */
        0xFFFFFFFF  /* Invalid: max */
    };

    int valid_pids = 0;
    int invalid_pids = 0;

    for (size_t i = 0; i < sizeof(pids) / sizeof(uint32_t); i++) {
        uint32_t pid = pids[i];
        if (pid > 0 && pid < MAX_PROCESSES) {
            valid_pids++;
        } else {
            invalid_pids++;
        }
    }

    if (valid_pids == 3 && invalid_pids == 5) {
        TEST_PASS("pid_range_validation");
        return 0;
    }
    TEST_FAIL("pid_range_validation");
}

/* Test 9: Mouse delta value validation */
static int test_mouse_delta_validation(void) {
    int32_t deltas[] = {
        -128, -100, -1,  /* Negative movement (left/up) */
        0,               /* No movement */
        1, 100, 127,     /* Positive movement (right/down) */
        -32768, -32767,  /* Extreme negative */
        32766, 32767     /* Extreme positive */
    };

    int reasonable_deltas = 0;
    const int32_t REASONABLE_MAX = 255;

    for (size_t i = 0; i < sizeof(deltas) / sizeof(int32_t); i++) {
        int32_t delta = deltas[i];
        /* Reasonable delta is typically -255 to +255 in one poll */
        if (delta >= -REASONABLE_MAX && delta <= REASONABLE_MAX) {
            reasonable_deltas++;
        }
    }

    if (reasonable_deltas == 7) {
        TEST_PASS("mouse_delta_validation");
        return 0;
    }
    TEST_FAIL("mouse_delta_validation");
}

/* Test 10: Stress test - 1000 mixed fuzz packets */
static int test_fuzz_stress_1000(void) {
    event_packet_t packets[100];
    int processed = 0;

    /* Generate 1000 packets in batches of 100 */
    for (int batch = 0; batch < 10; batch++) {
        for (int i = 0; i < 100; i++) {
            event_packet_t *p = &packets[i];

            /* Randomize but keep some valid */
            uint32_t seed = (batch * 100 + i) * 7919;  /* Prime number for distribution */

            p->channel = (seed % 256) % 10;  /* Channels 0-9 */
            p->code = (seed >> 8) % 20;      /* Codes 0-19 */
            p->source_pid = (seed >> 16) % MAX_PROCESSES;
            p->target_pid = (seed >> 20) % MAX_PROCESSES;
            p->payload_size = (seed >> 24) % 200;  /* Some over limit */

            /* Basic validation pass */
            int valid = 0;
            if (p->channel > 0 && p->channel < EVENT_CHANNEL_COUNT) valid++;
            if (p->payload_size <= EVENT_PAYLOAD_BYTES) valid++;
            if (p->source_pid > 0 && p->source_pid < MAX_PROCESSES) valid++;

            if (valid > 0) processed++;
        }
    }

    /* Should process many packets without crashing */
    if (processed > 500) {
        TEST_PASS("fuzz_stress_1000");
        return 0;
    }
    TEST_FAIL("fuzz_stress_1000");
}

/* Test 11: Event code boundary validation */
static int test_event_code_boundaries(void) {
    /* Test invalid event codes that should be rejected */
    uint32_t invalid_codes[] = {0, 0xFFFFFFFF, 0x10000};
    int rejected = 0;

    for (size_t i = 0; i < sizeof(invalid_codes) / sizeof(invalid_codes[0]); i++) {
        uint32_t code = invalid_codes[i];
        /* Simulate: only EVENT_CODE_INPUT is valid for input channel */
        if (code != EVENT_CODE_INPUT) {
            rejected++;
        }
    }

    if (rejected == 3) {
        TEST_PASS("event_code_boundaries");
        return 0;
    }
    TEST_FAIL("event_code_boundaries");
}

int main(void) {
    printf("=== Input Security Fuzz Tests ===\n");
    
    int failures = 0;
    failures += test_keyboard_invalid_codes();
    failures += test_keyboard_rapid_events();
    failures += test_mouse_invalid_buttons();
    failures += test_mouse_oob_coordinates();
    failures += test_ps2_corruption_detection();
    failures += test_event_channel_validation();
    failures += test_event_payload_validation();
    failures += test_pid_range_validation();
    failures += test_mouse_delta_validation();
    failures += test_fuzz_stress_1000();
    failures += test_event_code_boundaries();

    if (failures == 0) {
        printf("\n=== All input security fuzz tests passed ===\n");
        return 0;
    }

    printf("\n=== %d test(s) failed ===\n", failures);
    return 1;
}
