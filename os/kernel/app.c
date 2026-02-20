#include "app.h"

#include "diag.h"
#include "event_bus.h"
#include "heap.h"
#include "input.h"
#include "process.h"
#include "wm.h"

#define MAX_APPS 16
#define APP_EVENTS_PER_STEP 12

typedef struct {
    app_manifest_t manifest;
    UINT32 pid;
    UINT32 window_id;
    UINT64 ticks;
    CHAR16 content[96];
    UINTN content_len;
} app_instance_t;

static app_manifest_t g_manifests[MAX_APPS];
static UINTN g_manifest_count;
static app_instance_t *g_instances[MAX_APPS];
static UINTN g_instance_count;

static void copy_chars(CHAR16 *dst, const CHAR16 *src, UINTN max_chars) {
    if (max_chars == 0) {
        return;
    }

    UINTN i = 0;
    while (i + 1 < max_chars && src[i] != 0) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = 0;
}

static BOOLEAN equals_chars(const CHAR16 *a, const CHAR16 *b) {
    UINTN i = 0;
    for (;;) {
        if (a[i] != b[i]) {
            return FALSE;
        }
        if (a[i] == 0) {
            return TRUE;
        }
        ++i;
    }
}

static void append_char(app_instance_t *instance, CHAR16 ch) {
    if (instance == NULL) {
        return;
    }

    if (instance->content_len + 1 >= 96) {
        return;
    }

    instance->content[instance->content_len++] = ch;
    instance->content[instance->content_len] = 0;
}

static void trim_char(app_instance_t *instance) {
    if (instance == NULL || instance->content_len == 0) {
        return;
    }

    --instance->content_len;
    instance->content[instance->content_len] = 0;
}

static void copy_text(CHAR16 *dst, const CHAR16 *src, UINTN max_chars) {
    if (dst == NULL || src == NULL || max_chars == 0) {
        return;
    }

    UINTN i = 0;
    while (i + 1 < max_chars && src[i] != 0) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = 0;
}

static void to_decimal(UINT64 value, CHAR16 *out, UINTN max_chars) {
    if (out == NULL || max_chars == 0) {
        return;
    }

    if (value == 0) {
        out[0] = L'0';
        if (max_chars > 1) {
            out[1] = 0;
        }
        return;
    }

    CHAR16 tmp[24];
    UINTN len = 0;
    while (value > 0 && len < 23) {
        tmp[len++] = (CHAR16)(L'0' + (value % 10));
        value /= 10;
    }

    UINTN out_index = 0;
    while (len > 0 && out_index + 1 < max_chars) {
        out[out_index++] = tmp[--len];
    }
    out[out_index] = 0;
}

static void copy_append_text(CHAR16 *dst, UINTN max_chars, const CHAR16 *src) {
    if (dst == NULL || src == NULL || max_chars == 0) {
        return;
    }

    UINTN len = 0;
    while (len + 1 < max_chars && dst[len] != 0) {
        ++len;
    }

    UINTN i = 0;
    while (len + 1 < max_chars && src[i] != 0) {
        dst[len++] = src[i++];
    }
    dst[len] = 0;
}

static void to_hex32(UINT32 value, CHAR16 *out, UINTN max_chars) {
    static const CHAR16 digits[] = L"0123456789ABCDEF";
    if (out == NULL || max_chars < 3) {
        return;
    }

    out[0] = L'0';
    out[1] = L'x';
    UINTN written = 2;
    BOOLEAN started = FALSE;

    for (INT32 shift = 28; shift >= 0; shift -= 4) {
        UINT32 nibble = (value >> (UINT32)shift) & 0xFU;
        if (!started && nibble == 0 && shift > 0) {
            continue;
        }
        started = TRUE;
        if (written + 1 >= max_chars) {
            break;
        }
        out[written++] = digits[nibble];
    }

    if (!started && written + 1 < max_chars) {
        out[written++] = L'0';
    }

    out[written] = 0;
}

static void set_default_content(app_instance_t *instance) {
    if (instance == NULL) {
        return;
    }

    if (equals_chars(instance->manifest.id, L"shell")) {
        copy_text(instance->content, L"Shell ready. Type commands.", 96);
    } else if (equals_chars(instance->manifest.id, L"files")) {
        copy_text(instance->content, L"/apps: shell files term settings tasks", 96);
    } else if (equals_chars(instance->manifest.id, L"term")) {
        copy_text(instance->content, L"> ", 96);
    } else if (equals_chars(instance->manifest.id, L"settings")) {
        copy_text(instance->content, L"Settings: click to focus, type to edit.", 96);
    } else if (equals_chars(instance->manifest.id, L"tasks")) {
        copy_text(instance->content, L"Task manager online.", 96);
    } else {
        copy_text(instance->content, L"App online.", 96);
    }

    instance->content_len = 0;
    while (instance->content_len < 95 && instance->content[instance->content_len] != 0) {
        ++instance->content_len;
    }
}

static void handle_app_input(app_instance_t *instance, const input_event_t *input) {
    if (instance == NULL || input == NULL) {
        return;
    }

    if (input->type != INPUT_EVENT_KEY_DOWN) {
        return;
    }

    CHAR16 ch = input->data.key.unicode;
    UINT16 scan = input->data.key.scan_code;

    if (scan == SCAN_DELETE || ch == 0x0008) {
        trim_char(instance);
        return;
    }

    if (ch == L'\r') {
        if (equals_chars(instance->manifest.id, L"shell")) {
            copy_text(instance->content, L"Shell: command executed", 96);
        } else if (equals_chars(instance->manifest.id, L"term")) {
            copy_text(instance->content, L"> command accepted", 96);
        }

        instance->content_len = 0;
        while (instance->content_len < 95 && instance->content[instance->content_len] != 0) {
            ++instance->content_len;
        }
        return;
    }

    if (ch >= 32 && ch <= 126) {
        append_char(instance, ch);
    }
}

static BOOLEAN app_task_step(void *context) {
    app_instance_t *instance = (app_instance_t *)context;
    if (instance == NULL) {
        return FALSE;
    }

    event_packet_t packet;
    UINTN processed = 0;
    while (processed < APP_EVENTS_PER_STEP && event_bus_receive(instance->pid, &packet)) {
        if (packet.code == EVENT_CODE_APP_INPUT && packet.payload_size >= sizeof(input_event_t)) {
            input_event_t input;
            UINT8 *dst = (UINT8 *)&input;
            for (UINTN i = 0; i < sizeof(input_event_t); ++i) {
                dst[i] = packet.payload[i];
            }
            handle_app_input(instance, &input);
        }
        ++processed;
    }

    ++instance->ticks;
    if ((instance->ticks % 120) == 0) {
        event_packet_t packet;
        packet.channel = EVENT_CHANNEL_APP;
        packet.code = EVENT_CODE_APP_HEARTBEAT;
        packet.source_pid = instance->pid;
        packet.target_pid = instance->pid;
        packet.target_window = instance->window_id;
        packet.payload_size = sizeof(UINT64);
        UINT8 *tick_bytes = (UINT8 *)&instance->ticks;
        for (UINTN i = 0; i < sizeof(UINT64); ++i) {
            packet.payload[i] = tick_bytes[i];
        }
        for (UINTN i = 0; i < EVENT_PAYLOAD_BYTES; ++i) {
            if (i >= sizeof(UINT64)) {
                packet.payload[i] = 0;
            }
        }
        (void)event_bus_publish(&packet);
    }

    if (equals_chars(instance->manifest.id, L"tasks")) {
        CHAR16 count_buf[24];
        CHAR16 text[96];
        to_decimal((UINT64)process_running_count(), count_buf, 24);
        copy_text(text, L"Tasks running: ", 96);
        UINTN len = 0;
        while (len < 95 && text[len] != 0) {
            ++len;
        }
        UINTN i = 0;
        while (len + 1 < 96 && count_buf[i] != 0) {
            text[len++] = count_buf[i++];
        }
        text[len] = 0;
        copy_text(instance->content, text, 96);
        instance->content_len = len;
    } else if (equals_chars(instance->manifest.id, L"logs")) {
        diag_record_t latest;
        CHAR16 domain_buf[20];
        CHAR16 code_buf[20];
        copy_text(instance->content, L"Logs: waiting", 96);

        if (diag_latest(&latest)) {
            to_hex32(latest.domain, domain_buf, 20);
            to_hex32(latest.code, code_buf, 20);

            copy_text(instance->content, L"Log ", 96);
            copy_append_text(instance->content, 96, domain_buf);
            copy_append_text(instance->content, 96, L"/");
            copy_append_text(instance->content, 96, code_buf);
        }

        instance->content_len = 0;
        while (instance->content_len < 95 && instance->content[instance->content_len] != 0) {
            ++instance->content_len;
        }
    }

    (void)wm_set_window_content(instance->window_id, instance->content);

    return TRUE;
}

void app_framework_init(void) {
    g_manifest_count = 0;
    g_instance_count = 0;
    for (UINTN i = 0; i < MAX_APPS; ++i) {
        g_instances[i] = NULL;
    }
}

BOOLEAN app_register(const app_manifest_t *manifest) {
    if (manifest == NULL || g_manifest_count >= MAX_APPS) {
        return FALSE;
    }

    g_manifests[g_manifest_count] = *manifest;
    ++g_manifest_count;
    return TRUE;
}

BOOLEAN app_launch(const CHAR16 *id) {
    if (id == NULL) {
        return FALSE;
    }

    if (g_instance_count >= MAX_APPS) {
        return FALSE;
    }

    for (UINTN i = 0; i < g_instance_count; ++i) {
        app_instance_t *instance = g_instances[i];
        if (instance == NULL) {
            continue;
        }

        if (!equals_chars(instance->manifest.id, id)) {
            continue;
        }

        if (process_is_running(instance->pid)) {
            return wm_focus_window(instance->window_id);
        }
    }

    for (UINTN i = 0; i < g_manifest_count; ++i) {
        if (!equals_chars(g_manifests[i].id, id)) {
            continue;
        }

        app_instance_t *instance = (app_instance_t *)heap_alloc(sizeof(app_instance_t));
        if (instance == NULL) {
            return FALSE;
        }

        instance->manifest = g_manifests[i];
        instance->ticks = 0;
        instance->content[0] = 0;
        instance->content_len = 0;
        instance->pid = process_create_kernel(instance->manifest.title, app_task_step, instance, 2, instance->manifest.capabilities);
        if (instance->pid == 0) {
            return FALSE;
        }

        instance->window_id = wm_create_window(
            instance->pid,
            instance->manifest.title,
            instance->manifest.start_x,
            instance->manifest.start_y,
            instance->manifest.width,
            instance->manifest.height
        );
        if (instance->window_id == 0) {
            process_exit(instance->pid, -2);
            return FALSE;
        }

        set_default_content(instance);
        (void)wm_set_window_content(instance->window_id, instance->content);

        g_instances[g_instance_count++] = instance;

        return instance->window_id != 0;
    }

    return FALSE;
}

void app_launch_core_suite(void) {
    const app_manifest_t defaults[] = {
        { L"shell", L"System Shell", CAP_SYSTEM | CAP_INPUT, 90, 80, 420, 280 },
        { L"files", L"File Browser", CAP_STORAGE | CAP_GRAPHICS, 160, 120, 420, 300 },
        { L"term", L"Terminal", CAP_SYSTEM | CAP_INPUT, 240, 150, 460, 300 },
        { L"settings", L"Settings", CAP_GRAPHICS, 320, 180, 360, 260 },
        { L"tasks", L"Task Manager", CAP_SYSTEM, 400, 210, 360, 260 },
        { L"logs", L"System Logs", CAP_SYSTEM, 460, 240, 380, 160 }
    };

    for (UINTN i = 0; i < (sizeof(defaults) / sizeof(defaults[0])); ++i) {
        app_manifest_t manifest;
        copy_chars(manifest.id, defaults[i].id, 24);
        copy_chars(manifest.title, defaults[i].title, 32);
        manifest.capabilities = defaults[i].capabilities;
        manifest.start_x = defaults[i].start_x;
        manifest.start_y = defaults[i].start_y;
        manifest.width = defaults[i].width;
        manifest.height = defaults[i].height;
        (void)app_register(&manifest);
        (void)app_launch(manifest.id);
    }
}