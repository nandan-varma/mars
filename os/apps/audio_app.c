#include "uefi.h"
#include "app.h"
#include "input.h"
#include "process.h"
#include "scheduler.h"
#include "heap.h"
#include "audio.h"
#include "timer.h"
#include "wm.h"
#include "app_internal.h"

typedef struct {
    CHAR16 content[512];
    UINT32 content_len;
    BOOLEAN speaker_available;
    UINT32 beep_count;
    UINTN tick_count;
} audio_app_state_t;

static audio_app_state_t *g_audio_state = NULL;

static void update_audio_info(void) {
    if (g_audio_state == NULL) {
        return;
    }

    UINTN idx = 0;

    g_audio_state->content[idx++] = L'A';
    g_audio_state->content[idx++] = L'u';
    g_audio_state->content[idx++] = L'd';
    g_audio_state->content[idx++] = L'i';
    g_audio_state->content[idx++] = L'o';
    g_audio_state->content[idx++] = L' ';
    g_audio_state->content[idx++] = L'T';
    g_audio_state->content[idx++] = L'e';
    g_audio_state->content[idx++] = L's';
    g_audio_state->content[idx++] = L't';
    g_audio_state->content[idx++] = L':';
    g_audio_state->content[idx++] = L'\n';
    g_audio_state->content[idx++] = L'\n';

    if (g_audio_state->speaker_available) {
        const CHAR16 *status = L"PC Speaker: OK\n\n";
        for (UINTN i = 0; status[i] != 0 && idx < 500; ++i) {
            g_audio_state->content[idx++] = status[i];
        }

        const CHAR16 *beep_str = L"Beeps: ";
        for (UINTN i = 0; beep_str[i] != 0 && idx < 500; ++i) {
            g_audio_state->content[idx++] = beep_str[i];
        }

        UINT32 count = g_audio_state->beep_count;
        CHAR16 count_buf[8];
        UINTN count_idx = 0;
        if (count == 0) {
            count_buf[count_idx++] = L'0';
        } else {
            CHAR16 rev[8];
            UINTN rev_idx = 0;
            while (count > 0) {
                rev[rev_idx++] = L'0' + (count % 10);
                count /= 10;
            }
            while (rev_idx > 0) {
                count_buf[count_idx++] = rev[--rev_idx];
            }
        }
        count_buf[count_idx] = 0;
        for (UINTN i = 0; count_buf[i] != 0 && idx < 500; ++i) {
            g_audio_state->content[idx++] = count_buf[i];
        }
        g_audio_state->content[idx++] = L'\n';

        const CHAR16 *controls = L"\nFrequencies:\n1: 100 Hz\n2: 440 Hz\n3: 1000 Hz\n4: 2000 Hz\n\nPress 1-4 to play";
        for (UINTN i = 0; controls[i] != 0 && idx < 500; ++i) {
            g_audio_state->content[idx++] = controls[i];
        }
    } else {
        const CHAR16 *no_audio = L"No audio device\n\nUse real hardware or\nQEMU with HDA";
        for (UINTN i = 0; no_audio[i] != 0 && idx < 500; ++i) {
            g_audio_state->content[idx++] = no_audio[i];
        }
    }

    g_audio_state->content_len = idx;
}

BOOLEAN audio_app_init(UINT32 window_id) {
    (void)window_id;
    g_audio_state = heap_alloc(sizeof(audio_app_state_t));
    if (g_audio_state == NULL) {
        return FALSE;
    }

    g_audio_state->content[0] = 0;
    g_audio_state->content_len = 0;
    g_audio_state->speaker_available = FALSE;
    g_audio_state->beep_count = 0;
    g_audio_state->tick_count = 0;

    audio_init();
    g_audio_state->speaker_available = TRUE;

    update_audio_info();

    return TRUE;
}

void audio_app_render(app_instance_t *instance) {
    if (g_audio_state == NULL) {
        return;
    }

    g_audio_state->tick_count++;

    if (g_audio_state->tick_count % 60 == 0) {
        update_audio_info();
    }

    // Copy content to instance
    UINTN len = g_audio_state->content_len;
    if (len > APP_CONTENT_CHARS) len = APP_CONTENT_CHARS;
    for (UINTN i = 0; i < len; ++i) {
        instance->content[i] = g_audio_state->content[i];
    }
    instance->content[len] = 0;
    instance->content_len = (UINT32)len;
}

void audio_app_handle_input(const input_event_t *event) {
    if (g_audio_state == NULL || event == NULL) {
        return;
    }

    if (event->type == INPUT_EVENT_KEY_DOWN) {
        UINT16 freq = 0;
        switch (event->data.key.unicode) {
            case L'1': freq = 100; break;
            case L'2': freq = 440; break;
            case L'3': freq = 1000; break;
            case L'4': freq = 2000; break;
            default: break;
        }

        if (freq > 0 && g_audio_state->speaker_available) {
            if (audio_beep(freq, 200)) {
                g_audio_state->beep_count++;
                update_audio_info();
            }
        }
    }
}

extern void audio_app_register(void) {
    app_manifest_t manifest = {
        L"audio_app",
        L"Audio Test",
        CAP_INPUT,
        500, 200,
        380, 350
    };
    app_register(&manifest);
}