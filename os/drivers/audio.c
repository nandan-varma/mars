#include "audio.h"
#include "platform.h"
#include "diag.h"
#include "heap.h"

static UINT8 *g_audio_buffer;
static UINTN g_buffer_size;
static BOOLEAN g_initialized;
static BOOLEAN g_hda_available;
static BOOLEAN g_speaker_available;

static void io_out8(UINT16 port, UINT8 value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static UINT8 io_in8(UINT16 port) {
    UINT8 result;
    __asm__ volatile ("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

void audio_init(void) {
    if (g_initialized) {
        return;
    }

    g_initialized = TRUE;
    g_hda_available = FALSE;
    g_speaker_available = FALSE;

    g_buffer_size = AUDIO_SAMPLE_RATE * AUDIOChannels * (AUDIO_BITS_PER_SAMPLE / 8) / 2;
    g_audio_buffer = heap_alloc(g_buffer_size);

    if (g_audio_buffer == NULL) {
        diag_log(0x900U, 1, 0, 0);
        return;
    }

    g_speaker_available = audio_init_speaker();
    if (g_speaker_available) {
        diag_log(0x901U, 1, 0, 0);
    }

    if (audio_init_hda()) {
        g_hda_available = TRUE;
        diag_log(0x901U, 2, 0, 0);
    }

    diag_log(0x902U, g_speaker_available ? 1 : 0, g_hda_available ? 1 : 0, g_buffer_size);
}

BOOLEAN audio_init_hda(void) {
    return FALSE;
}

BOOLEAN audio_init_speaker(void) {
    UINT8 val = io_in8(PC_SPEAKER_PORT);
    if ((val & 0x03) == 0) {
        return TRUE;
    }

    io_out8(0x43, 0xB6);
    io_out8(0x42, 0xFF);
    io_out8(0x42, 0xFF);

    return TRUE;
}

BOOLEAN audio_play(const UINT8 *data, UINTN size) {
    if (data == NULL || size == 0) {
        return FALSE;
    }

    if (!g_hda_available && !g_speaker_available) {
        return FALSE;
    }

    return TRUE;
}

BOOLEAN audio_beep(UINT16 frequency_hz, UINT16 duration_ms) {
    if (!g_speaker_available) {
        return FALSE;
    }

    if (frequency_hz < 20 || frequency_hz > 20000) {
        frequency_hz = 1000;
    }

    UINT32 divisor = PC_SPEAKER_TIMER_FREQ / frequency_hz;

    io_out8(0x43, 0xB6);

    io_out8(0x42, (UINT8)(divisor & 0xFF));
    io_out8(0x42, (UINT8)((divisor >> 8) & 0xFF));

    UINT8 val = io_in8(PC_SPEAKER_PORT);
    io_out8(PC_SPEAKER_PORT, val | 0x03);

    for (volatile UINTN i = 0; i < duration_ms * 1000; ++i) {
    }

    val = io_in8(PC_SPEAKER_PORT);
    io_out8(PC_SPEAKER_PORT, val & 0xFC);

    diag_log(0x903U, frequency_hz, duration_ms, 0);
    return TRUE;
}

void audio_stop(void) {
    if (!g_speaker_available) {
        return;
    }

    UINT8 val = io_in8(PC_SPEAKER_PORT);
    io_out8(PC_SPEAKER_PORT, val & 0xFC);
}