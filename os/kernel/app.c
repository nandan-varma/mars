#include "app.h"
#include "app_utils.h"
#include "app_console_cmd.h"
#include "internal/app_internal.h"

#include "diag.h"
#include "event_bus.h"
#include "input.h"
#include "process.h"
#include "scheduler.h"
#include "timer.h"
#include "vfs.h"
#include "wm.h"

#define APP_MAX_TASK_LINES 6
#define APP_MAX_LOG_LINES 8

static UINT32 g_app_manager_pid;

// ============================================================================
// SDK App Forward Declarations
// ============================================================================

extern BOOLEAN disk_app_init(UINT32 window_id);
extern void disk_app_render(app_instance_t *instance);
extern void disk_app_handle_input(const input_event_t *event);

extern BOOLEAN network_app_init(UINT32 window_id);
extern void network_app_render(app_instance_t *instance);
extern void network_app_handle_input(const input_event_t *event);

extern BOOLEAN audio_app_init(UINT32 window_id);
extern void audio_app_render(app_instance_t *instance);
extern void audio_app_handle_input(const input_event_t *event);

extern BOOLEAN sysinfo_app_init(UINT32 window_id);
extern void sysinfo_app_render(app_instance_t *instance);
extern void sysinfo_app_handle_input(const input_event_t *event);

static void reset_input(app_instance_t *instance) {
    if (instance == NULL) {
        return;
    }

    instance->input_len = 0;
    instance->input_line[0] = 0;
}

static BOOLEAN is_console_id(const CHAR16 *id) {
    return app_utils_equals(id, L"shell") || app_utils_equals(id, L"term") || app_utils_equals(id, L"files") || app_utils_equals(id, L"settings");
}

static const CHAR16 *prompt_for(const CHAR16 *id) {
    if (app_utils_equals(id, L"shell")) {
        return L"shell> ";
    }
    if (app_utils_equals(id, L"term")) {
        return L"term> ";
    }
    if (app_utils_equals(id, L"files")) {
        return L"files> ";
    }
    if (app_utils_equals(id, L"settings")) {
        return L"settings> ";
    }
    return L"> ";
}

static void compose_console_view(app_instance_t *instance) {
    if (instance == NULL) {
        return;
    }

    instance->content[0] = 0;
    instance->content_len = 0;

    app_utils_buffer_append_text(instance->content, APP_CONTENT_CHARS, &instance->content_len, instance->history);
    if (instance->content_len > 0) {
        app_utils_buffer_append_char(instance->content, APP_CONTENT_CHARS, &instance->content_len, L'\n');
    }
    app_utils_buffer_append_text(instance->content, APP_CONTENT_CHARS, &instance->content_len, prompt_for(instance->manifest.id));
    app_utils_buffer_append_text(instance->content, APP_CONTENT_CHARS, &instance->content_len, instance->input_line);
}

static void history_append_line(app_instance_t *instance, const CHAR16 *line) {
    if (instance == NULL || line == NULL) {
        return;
    }

    if (instance->history_len > 0) {
        app_utils_buffer_append_char(instance->history, APP_CONTENT_CHARS, &instance->history_len, L'\n');
    }
    app_utils_buffer_append_text(instance->history, APP_CONTENT_CHARS, &instance->history_len, line);
}

static void history_append_prompt_line(app_instance_t *instance) {
    if (instance == NULL) {
        return;
    }

    CHAR16 line[APP_INPUT_CHARS + 16];
    app_utils_copy_text(line, prompt_for(instance->manifest.id), APP_INPUT_CHARS + 16);
    app_utils_copy_append(line, APP_INPUT_CHARS + 16, instance->input_line);
    history_append_line(instance, line);
}

static void append_task_state(CHAR16 *line, UINTN max_chars, task_state_t state) {
    if (state == TASK_READY) {
        app_utils_copy_append(line, max_chars, L"ready");
    } else if (state == TASK_RUNNING) {
        app_utils_copy_append(line, max_chars, L"run");
    } else if (state == TASK_BLOCKED) {
        app_utils_copy_append(line, max_chars, L"blk");
    } else {
        app_utils_copy_append(line, max_chars, L"stop");
    }
}

static void render_tasks_content(app_instance_t *instance) {
    if (instance == NULL) {
        return;
    }

    instance->content[0] = 0;
    instance->content_len = 0;

    CHAR16 line[96];
    app_utils_copy_text(line, L"Running processes: ", 96);
    app_utils_append_u64(line, 96, process_running_count());
    app_utils_buffer_append_text(instance->content, APP_CONTENT_CHARS, &instance->content_len, line);

    app_utils_buffer_append_char(instance->content, APP_CONTENT_CHARS, &instance->content_len, L'\n');
    app_utils_copy_text(line, L"Tasks:", 96);
    app_utils_buffer_append_text(instance->content, APP_CONTENT_CHARS, &instance->content_len, line);

    UINTN shown = 0;
    UINTN total = scheduler_task_count();
    for (UINTN i = 0; i < total && shown < APP_MAX_TASK_LINES; ++i) {
        task_t task;
        if (!scheduler_task_at(i, &task)) {
            continue;
        }

        app_utils_buffer_append_char(instance->content, APP_CONTENT_CHARS, &instance->content_len, L'\n');
        line[0] = 0;
        app_utils_copy_append(line, 96, L"#");
        app_utils_append_u64(line, 96, task.id);
        app_utils_copy_append(line, 96, L" ");
        app_utils_copy_append(line, 96, task.name);
        app_utils_copy_append(line, 96, L" pid=");
        app_utils_append_u64(line, 96, task.owner_pid);
        app_utils_copy_append(line, 96, L" ");
        append_task_state(line, 96, task.state);
        app_utils_buffer_append_text(instance->content, APP_CONTENT_CHARS, &instance->content_len, line);
        ++shown;
    }
}

static void render_logs_content(app_instance_t *instance) {
    if (instance == NULL) {
        return;
    }

    instance->content[0] = 0;
    instance->content_len = 0;
    app_utils_buffer_append_text(instance->content, APP_CONTENT_CHARS, &instance->content_len, L"Recent kernel logs:");

    for (UINTN i = 0; i < APP_MAX_LOG_LINES; ++i) {
        diag_record_t rec;
        if (!diag_recent(i, &rec)) {
            if (i == 0) {
                app_utils_buffer_append_char(instance->content, APP_CONTENT_CHARS, &instance->content_len, L'\n');
                app_utils_buffer_append_text(instance->content, APP_CONTENT_CHARS, &instance->content_len, L"(no records yet)");
            }
            break;
        }

        CHAR16 line[96];
        CHAR16 d[20];
        CHAR16 c[20];
        app_utils_to_hex32(rec.domain, d, 20);
        app_utils_to_hex32(rec.code, c, 20);
        line[0] = 0;
        app_utils_copy_append(line, 96, L"t=");
        app_utils_append_u64(line, 96, rec.tick);
        app_utils_copy_append(line, 96, L" ");
        app_utils_copy_append(line, 96, d);
        app_utils_copy_append(line, 96, L"/");
        app_utils_copy_append(line, 96, c);
        app_utils_buffer_append_char(instance->content, APP_CONTENT_CHARS, &instance->content_len, L'\n');
        app_utils_buffer_append_text(instance->content, APP_CONTENT_CHARS, &instance->content_len, line);
    }
}

static void set_default_content(app_instance_t *instance) {
    if (instance == NULL) {
        return;
    }

    instance->content[0] = 0;
    instance->content_len = 0;
    instance->history[0] = 0;
    instance->history_len = 0;
    reset_input(instance);

    if (app_utils_equals(instance->manifest.id, L"shell")) {
        history_append_line(instance, L"Mars shell. Type help.");
        compose_console_view(instance);
    } else if (app_utils_equals(instance->manifest.id, L"term")) {
        history_append_line(instance, L"Terminal online. Type help.");
        compose_console_view(instance);
    } else if (app_utils_equals(instance->manifest.id, L"files")) {
        history_append_line(instance, L"File browser ready. Type ls or cat <path>.");
        compose_console_view(instance);
    } else if (app_utils_equals(instance->manifest.id, L"settings")) {
        history_append_line(instance, L"Settings console. Type status or debug toggle.");
        compose_console_view(instance);
    } else if (app_utils_equals(instance->manifest.id, L"tasks")) {
        render_tasks_content(instance);
    } else if (app_utils_equals(instance->manifest.id, L"logs")) {
        render_logs_content(instance);
    } else {
        app_utils_copy_text(instance->content, L"App online.", APP_CONTENT_CHARS);
        instance->content_len = app_utils_text_length(instance->content, APP_CONTENT_CHARS);
    }
}

static void handle_app_input(app_instance_t *instance, const input_event_t *input) {
    if (instance == NULL || input == NULL) {
        return;
    }

    if (input->type != INPUT_EVENT_KEY_DOWN) {
        return;
    }

    if (!is_console_id(instance->manifest.id)) {
        return;
    }

    CHAR16 ch = input->data.key.unicode;
    UINT16 scan = input->data.key.scan_code;

    if (scan == SCAN_DELETE || ch == 0x0008) {
        if (instance->input_len > 0) {
            --instance->input_len;
            instance->input_line[instance->input_len] = 0;
        }
        compose_console_view(instance);
        return;
    }

    if (ch == L'\r') {
        history_append_prompt_line(instance);
        app_execute_console_command(instance);
        reset_input(instance);
        compose_console_view(instance);
        return;
    }

    if (ch >= 32 && ch <= 126) {
        if (instance->input_len + 1 < APP_INPUT_CHARS) {
            instance->input_line[instance->input_len++] = ch;
            instance->input_line[instance->input_len] = 0;
        }
        compose_console_view(instance);
        return;
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
        // SECURITY FIX #6: Validate payload size matches expected input event size
        // Using >= allows reading uninitialized bytes from the payload buffer
        if (packet.code == EVENT_CODE_APP_INPUT && packet.payload_size == sizeof(input_event_t)) {
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

    if (app_utils_equals(instance->manifest.id, L"tasks") && (instance->ticks % 10) == 0) {
        render_tasks_content(instance);
    } else if (app_utils_equals(instance->manifest.id, L"logs") && (instance->ticks % 10) == 0) {
        render_logs_content(instance);
    }

    (void)wm_set_window_content(instance->window_id, instance->content);

    return TRUE;
}

static BOOLEAN app_manager_task(void *context) {
    (void)context;

    event_packet_t packet;
    UINTN processed = 0;
    while (processed < APP_EVENTS_PER_STEP && event_bus_receive_channel(EVENT_CHANNEL_SYSTEM, &packet)) {
        if (packet.code == EVENT_CODE_APP_LAUNCH_REQUEST && packet.payload_size >= sizeof(CHAR16)) {
            if (packet.source_pid != 0) {
                UINT32 caps = process_capabilities(packet.source_pid);
                if ((caps & CAP_SYSTEM) == 0) {
                    ++processed;
                    continue;
                }
            }

            CHAR16 id[24];
            UINTN id_bytes = packet.payload_size;
            if (id_bytes > sizeof(id) - sizeof(CHAR16)) {
                id_bytes = sizeof(id) - sizeof(CHAR16);
            }

            UINTN i = 0;
            for (; i < id_bytes; ++i) {
                ((UINT8 *)id)[i] = packet.payload[i];
            }
            for (; i < sizeof(id); ++i) {
                ((UINT8 *)id)[i] = 0;
            }
            id[(sizeof(id) / sizeof(id[0])) - 1] = 0;

            if (app_utils_bounded_len(id, 24) > 0) {
                (void)app_launch(id);
            }
        }
        ++processed;
    }

    return TRUE;
}

void app_framework_init(void) {
    app_registry_init();
    app_instance_init();
    g_app_manager_pid = 0;
    g_app_manager_pid = process_create_kernel(L"app-manager", app_manager_task, NULL, 1, CAP_SYSTEM, TRUE);
}

BOOLEAN app_register(const app_manifest_t *manifest) {
    return app_registry_register(manifest);
}

// ============================================================================
// SDK App Task Implementation
// ============================================================================

static BOOLEAN sdk_app_task(void *context) {
    app_instance_t *instance = (app_instance_t *)context;
    if (instance == NULL) {
        return FALSE;
    }

    // Use first byte of history to track initialization
    BOOLEAN *initialized = (BOOLEAN *)&instance->history[0];
    
    // Initialize on first run
    if (!*initialized) {
        BOOLEAN (*init_fn)(UINT32) = NULL;

        if (app_utils_equals(instance->manifest.id, L"disk_app")) {
            init_fn = disk_app_init;
        } else if (app_utils_equals(instance->manifest.id, L"network_app")) {
            init_fn = network_app_init;
        } else if (app_utils_equals(instance->manifest.id, L"audio_app")) {
            init_fn = audio_app_init;
        } else if (app_utils_equals(instance->manifest.id, L"sysinfo_app")) {
            init_fn = sysinfo_app_init;
        }

        if (init_fn != NULL && init_fn(instance->window_id)) {
            *initialized = TRUE;
        } else {
            return FALSE;
        }
    }

    // Handle input events
    event_packet_t packet;
    UINTN processed = 0;
    while (processed < APP_EVENTS_PER_STEP && event_bus_receive(instance->pid, &packet)) {
        if (packet.code == EVENT_CODE_APP_INPUT && packet.payload_size == sizeof(input_event_t)) {
            input_event_t input;
            UINT8 *dst = (UINT8 *)&input;
            for (UINTN i = 0; i < sizeof(input_event_t); ++i) {
                dst[i] = packet.payload[i];
            }

            // Dispatch input to appropriate app
            if (app_utils_equals(instance->manifest.id, L"disk_app")) {
                disk_app_handle_input(&input);
            } else if (app_utils_equals(instance->manifest.id, L"network_app")) {
                network_app_handle_input(&input);
            } else if (app_utils_equals(instance->manifest.id, L"audio_app")) {
                audio_app_handle_input(&input);
            } else if (app_utils_equals(instance->manifest.id, L"sysinfo_app")) {
                sysinfo_app_handle_input(&input);
            }
        }
        ++processed;
    }

    // SDK apps call their render functions to update content
    // Render functions update instance->content which is displayed by WM
    if ((instance->ticks % 10) == 0) {
        if (app_utils_equals(instance->manifest.id, L"disk_app")) {
            disk_app_render(instance);
        } else if (app_utils_equals(instance->manifest.id, L"network_app")) {
            network_app_render(instance);
        } else if (app_utils_equals(instance->manifest.id, L"audio_app")) {
            audio_app_render(instance);
        } else if (app_utils_equals(instance->manifest.id, L"sysinfo_app")) {
            sysinfo_app_render(instance);
        }
    }

    // Copy to WM
    (void)wm_set_window_content(instance->window_id, instance->content);

    return TRUE;
}

static void set_sdk_app_content(app_instance_t *instance) {
    if (instance == NULL) {
        return;
    }
    instance->content[0] = 0;
    instance->content_len = 0;
    instance->history[0] = 0;
    instance->history_len = 0;
    reset_input(instance);
}

BOOLEAN app_launch(const CHAR16 *id) {
    // Check if this is an SDK app
    if (app_utils_equals(id, L"disk_app") || app_utils_equals(id, L"network_app") ||
        app_utils_equals(id, L"audio_app") || app_utils_equals(id, L"sysinfo_app")) {
        return app_instance_launch(id, sdk_app_task, set_sdk_app_content);
    }
    // Legacy console apps
    return app_instance_launch(id, app_task_step, set_default_content);
}

void app_launch_core_suite(void) {
    const app_manifest_t defaults[] = {
        { L"disk_app", L"Disk Browser", CAP_GRAPHICS | CAP_STORAGE, 100, 50, 400, 350 },
        { L"network_app", L"Network", CAP_GRAPHICS | CAP_INPUT | CAP_SYSTEM, 150, 80, 400, 380 },
        { L"audio_app", L"Audio Test", CAP_GRAPHICS | CAP_INPUT, 200, 110, 380, 320 },
        { L"sysinfo_app", L"System Info", CAP_GRAPHICS, 250, 140, 380, 280 }
    };

    for (UINTN i = 0; i < (sizeof(defaults) / sizeof(defaults[0])); ++i) {
        app_manifest_t manifest;
        app_utils_copy_chars(manifest.id, defaults[i].id, 24);
        app_utils_copy_chars(manifest.title, defaults[i].title, 32);
        manifest.capabilities = defaults[i].capabilities;
        manifest.start_x = defaults[i].start_x;
        manifest.start_y = defaults[i].start_y;
        manifest.width = defaults[i].width;
        manifest.height = defaults[i].height;
        (void)app_register(&manifest);
        (void)app_launch(manifest.id);
    }
}