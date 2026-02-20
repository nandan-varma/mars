#include "app.h"

#include "diag.h"
#include "event_bus.h"
#include "heap.h"
#include "input.h"
#include "process.h"
#include "scheduler.h"
#include "timer.h"
#include "vfs.h"
#include "wm.h"

#define MAX_APPS 16
#define APP_EVENTS_PER_STEP 12
#define APP_CONTENT_CHARS 512
#define APP_INPUT_CHARS 96
#define APP_MAX_TASK_LINES 6
#define APP_MAX_LOG_LINES 8

typedef struct {
    app_manifest_t manifest;
    UINT32 pid;
    UINT32 window_id;
    UINT64 ticks;
    CHAR16 content[APP_CONTENT_CHARS];
    UINTN content_len;
    CHAR16 history[APP_CONTENT_CHARS];
    UINTN history_len;
    CHAR16 input_line[APP_INPUT_CHARS];
    UINTN input_len;
} app_instance_t;

static app_manifest_t g_manifests[MAX_APPS];
static UINTN g_manifest_count;
static app_instance_t *g_instances[MAX_APPS];
static UINTN g_instance_count;
static UINT32 g_app_manager_pid;

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

static BOOLEAN is_space(CHAR16 ch) {
    return ch == L' ' || ch == L'\t' || ch == L'\r' || ch == L'\n';
}

static UINTN text_length(const CHAR16 *text, UINTN max_chars) {
    if (text == NULL) {
        return 0;
    }

    UINTN len = 0;
    while (len < max_chars && text[len] != 0) {
        ++len;
    }
    return len;
}

static void buffer_trim_front(CHAR16 *buffer, UINTN max_chars, UINTN *len_io, UINTN needed_space) {
    if (buffer == NULL || len_io == NULL || max_chars == 0) {
        return;
    }

    UINTN len = *len_io;
    while (len + needed_space + 1 >= max_chars && len > 0) {
        UINTN trim = 0;
        while (trim < len && buffer[trim] != L'\n') {
            ++trim;
        }
        if (trim < len && buffer[trim] == L'\n') {
            ++trim;
        }
        if (trim == 0 || trim >= len) {
            len = 0;
            buffer[0] = 0;
            break;
        }

        UINTN write = 0;
        for (UINTN read = trim; read < len; ++read) {
            buffer[write++] = buffer[read];
        }
        len = write;
        buffer[len] = 0;
    }

    *len_io = len;
}

static void buffer_append_text(CHAR16 *buffer, UINTN max_chars, UINTN *len_io, const CHAR16 *text) {
    if (buffer == NULL || len_io == NULL || text == NULL || max_chars == 0) {
        return;
    }

    UINTN len = *len_io;
    UINTN add = text_length(text, max_chars);
    buffer_trim_front(buffer, max_chars, &len, add);

    UINTN i = 0;
    while (len + 1 < max_chars && text[i] != 0) {
        buffer[len++] = text[i++];
    }
    buffer[len] = 0;
    *len_io = len;
}

static void buffer_append_char(CHAR16 *buffer, UINTN max_chars, UINTN *len_io, CHAR16 ch) {
    if (buffer == NULL || len_io == NULL || max_chars == 0) {
        return;
    }

    UINTN len = *len_io;
    buffer_trim_front(buffer, max_chars, &len, 1);
    if (len + 1 < max_chars) {
        buffer[len++] = ch;
        buffer[len] = 0;
    }
    *len_io = len;
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

static UINTN bounded_text_len(const CHAR16 *text, UINTN max_chars) {
    UINTN len = 0;
    if (text == NULL) {
        return 0;
    }

    while (len < max_chars && text[len] != 0) {
        ++len;
    }
    return len;
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

static void append_u64_text(CHAR16 *dst, UINTN max_chars, UINT64 value) {
    CHAR16 tmp[24];
    to_decimal(value, tmp, 24);
    copy_append_text(dst, max_chars, tmp);
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

static void reset_input(app_instance_t *instance) {
    if (instance == NULL) {
        return;
    }

    instance->input_len = 0;
    instance->input_line[0] = 0;
}

static BOOLEAN is_console_id(const CHAR16 *id) {
    return equals_chars(id, L"shell") || equals_chars(id, L"term") || equals_chars(id, L"files") || equals_chars(id, L"settings");
}

static const CHAR16 *prompt_for(const CHAR16 *id) {
    if (equals_chars(id, L"shell")) {
        return L"shell> ";
    }
    if (equals_chars(id, L"term")) {
        return L"term> ";
    }
    if (equals_chars(id, L"files")) {
        return L"files> ";
    }
    if (equals_chars(id, L"settings")) {
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

    buffer_append_text(instance->content, APP_CONTENT_CHARS, &instance->content_len, instance->history);
    if (instance->content_len > 0) {
        buffer_append_char(instance->content, APP_CONTENT_CHARS, &instance->content_len, L'\n');
    }
    buffer_append_text(instance->content, APP_CONTENT_CHARS, &instance->content_len, prompt_for(instance->manifest.id));
    buffer_append_text(instance->content, APP_CONTENT_CHARS, &instance->content_len, instance->input_line);
}

static void history_append_line(app_instance_t *instance, const CHAR16 *line) {
    if (instance == NULL || line == NULL) {
        return;
    }

    if (instance->history_len > 0) {
        buffer_append_char(instance->history, APP_CONTENT_CHARS, &instance->history_len, L'\n');
    }
    buffer_append_text(instance->history, APP_CONTENT_CHARS, &instance->history_len, line);
}

static void history_append_prompt_line(app_instance_t *instance) {
    if (instance == NULL) {
        return;
    }

    CHAR16 line[APP_INPUT_CHARS + 16];
    copy_text(line, prompt_for(instance->manifest.id), APP_INPUT_CHARS + 16);
    copy_append_text(line, APP_INPUT_CHARS + 16, instance->input_line);
    history_append_line(instance, line);
}

static void append_task_state(CHAR16 *line, UINTN max_chars, task_state_t state) {
    if (state == TASK_READY) {
        copy_append_text(line, max_chars, L"ready");
    } else if (state == TASK_RUNNING) {
        copy_append_text(line, max_chars, L"run");
    } else if (state == TASK_BLOCKED) {
        copy_append_text(line, max_chars, L"blk");
    } else {
        copy_append_text(line, max_chars, L"stop");
    }
}

static void render_tasks_content(app_instance_t *instance) {
    if (instance == NULL) {
        return;
    }

    instance->content[0] = 0;
    instance->content_len = 0;

    CHAR16 line[96];
    copy_text(line, L"Running processes: ", 96);
    append_u64_text(line, 96, process_running_count());
    buffer_append_text(instance->content, APP_CONTENT_CHARS, &instance->content_len, line);

    buffer_append_char(instance->content, APP_CONTENT_CHARS, &instance->content_len, L'\n');
    copy_text(line, L"Tasks:", 96);
    buffer_append_text(instance->content, APP_CONTENT_CHARS, &instance->content_len, line);

    UINTN shown = 0;
    UINTN total = scheduler_task_count();
    for (UINTN i = 0; i < total && shown < APP_MAX_TASK_LINES; ++i) {
        task_t task;
        if (!scheduler_task_at(i, &task)) {
            continue;
        }

        buffer_append_char(instance->content, APP_CONTENT_CHARS, &instance->content_len, L'\n');
        line[0] = 0;
        copy_append_text(line, 96, L"#");
        append_u64_text(line, 96, task.id);
        copy_append_text(line, 96, L" ");
        copy_append_text(line, 96, task.name);
        copy_append_text(line, 96, L" pid=");
        append_u64_text(line, 96, task.owner_pid);
        copy_append_text(line, 96, L" ");
        append_task_state(line, 96, task.state);
        buffer_append_text(instance->content, APP_CONTENT_CHARS, &instance->content_len, line);
        ++shown;
    }
}

static void render_logs_content(app_instance_t *instance) {
    if (instance == NULL) {
        return;
    }

    instance->content[0] = 0;
    instance->content_len = 0;
    buffer_append_text(instance->content, APP_CONTENT_CHARS, &instance->content_len, L"Recent kernel logs:");

    for (UINTN i = 0; i < APP_MAX_LOG_LINES; ++i) {
        diag_record_t rec;
        if (!diag_recent(i, &rec)) {
            if (i == 0) {
                buffer_append_char(instance->content, APP_CONTENT_CHARS, &instance->content_len, L'\n');
                buffer_append_text(instance->content, APP_CONTENT_CHARS, &instance->content_len, L"(no records yet)");
            }
            break;
        }

        CHAR16 line[96];
        CHAR16 d[20];
        CHAR16 c[20];
        to_hex32(rec.domain, d, 20);
        to_hex32(rec.code, c, 20);
        line[0] = 0;
        copy_append_text(line, 96, L"t=");
        append_u64_text(line, 96, rec.tick);
        copy_append_text(line, 96, L" ");
        copy_append_text(line, 96, d);
        copy_append_text(line, 96, L"/");
        copy_append_text(line, 96, c);
        buffer_append_char(instance->content, APP_CONTENT_CHARS, &instance->content_len, L'\n');
        buffer_append_text(instance->content, APP_CONTENT_CHARS, &instance->content_len, line);
    }
}

static void files_ls(app_instance_t *instance) {
    if (instance == NULL) {
        return;
    }

    history_append_line(instance, L"Entries:");
    const CHAR16 *paths[] = {
        L"/apps/shell.manifest",
        L"/apps/files.manifest",
        L"/apps/term.manifest"
    };

    for (UINTN i = 0; i < (sizeof(paths) / sizeof(paths[0])); ++i) {
        CHAR16 line[96];
        line[0] = 0;
        copy_append_text(line, 96, vfs_exists(paths[i]) ? L"- " : L"? ");
        copy_append_text(line, 96, paths[i]);
        history_append_line(instance, line);
    }
}

static void files_cat(app_instance_t *instance, const CHAR16 *path) {
    if (instance == NULL || path == NULL || path[0] == 0) {
        history_append_line(instance, L"cat: missing path");
        return;
    }

    UINT8 bytes[80];
    UINTN read = vfs_read(path, bytes, 79);
    if (read == 0) {
        history_append_line(instance, L"cat: file not found");
        return;
    }

    CHAR16 line[96];
    line[0] = 0;
    UINTN line_len = 0;
    for (UINTN i = 0; i < read && i < 79; ++i) {
        CHAR16 ch = (CHAR16)bytes[i];
        if (ch == L'\n' || ch == L'\r') {
            ch = L' ';
        }
        buffer_append_char(line, 96, &line_len, ch);
    }
    history_append_line(instance, line);
}

static void parse_command(const CHAR16 *input, CHAR16 *cmd, UINTN cmd_max, CHAR16 *arg, UINTN arg_max) {
    if (cmd != NULL && cmd_max > 0) {
        cmd[0] = 0;
    }
    if (arg != NULL && arg_max > 0) {
        arg[0] = 0;
    }
    if (input == NULL || cmd == NULL || cmd_max == 0) {
        return;
    }

    UINTN i = 0;
    while (input[i] != 0 && is_space(input[i])) {
        ++i;
    }

    UINTN c = 0;
    while (input[i] != 0 && !is_space(input[i]) && c + 1 < cmd_max) {
        cmd[c++] = input[i++];
    }
    cmd[c] = 0;

    while (input[i] != 0 && is_space(input[i])) {
        ++i;
    }

    if (arg == NULL || arg_max == 0) {
        return;
    }

    UINTN a = 0;
    while (input[i] != 0 && a + 1 < arg_max) {
        arg[a++] = input[i++];
    }
    arg[a] = 0;
}

static void execute_shell_like(app_instance_t *instance) {
    if (instance == NULL) {
        return;
    }

    CHAR16 cmd[24];
    CHAR16 arg[64];
    parse_command(instance->input_line, cmd, 24, arg, 64);

    if (cmd[0] == 0) {
        return;
    }

    if (equals_chars(cmd, L"help")) {
        if (equals_chars(instance->manifest.id, L"files")) {
            history_append_line(instance, L"help | ls | cat <path> | exists <path> | clear");
        } else if (equals_chars(instance->manifest.id, L"settings")) {
            history_append_line(instance, L"help | status | debug on|off|toggle | clear");
        } else {
            history_append_line(instance, L"help | clear | tick | mem | ps | launch <app>");
            history_append_line(instance, L"apps | ls | cat <path> | echo <text>");
        }
        return;
    }

    if (equals_chars(cmd, L"clear")) {
        instance->history[0] = 0;
        instance->history_len = 0;
        return;
    }

    if (equals_chars(cmd, L"tick")) {
        CHAR16 line[64];
        copy_text(line, L"tick=", 64);
        append_u64_text(line, 64, timer_ticks());
        history_append_line(instance, line);
        return;
    }

    if (equals_chars(cmd, L"mem")) {
        CHAR16 line[96];
        line[0] = 0;
        copy_append_text(line, 96, L"heap ");
        append_u64_text(line, 96, heap_used_bytes() / 1024);
        copy_append_text(line, 96, L"/");
        append_u64_text(line, 96, heap_total_bytes() / 1024);
        copy_append_text(line, 96, L" KB");
        history_append_line(instance, line);
        return;
    }

    if (equals_chars(cmd, L"ps")) {
        CHAR16 line[96];
        line[0] = 0;
        copy_append_text(line, 96, L"proc=");
        append_u64_text(line, 96, process_running_count());
        copy_append_text(line, 96, L" task=");
        append_u64_text(line, 96, scheduler_task_count());
        history_append_line(instance, line);
        return;
    }

    if (equals_chars(cmd, L"apps")) {
        history_append_line(instance, L"shell files term settings tasks logs");
        return;
    }

    if (equals_chars(cmd, L"echo")) {
        history_append_line(instance, arg[0] == 0 ? L"" : arg);
        return;
    }

    if (equals_chars(cmd, L"launch") || equals_chars(cmd, L"open")) {
        if (arg[0] == 0) {
            history_append_line(instance, L"launch: missing app id");
        } else if (app_launch(arg)) {
            history_append_line(instance, L"launch: ok");
        } else {
            history_append_line(instance, L"launch: failed");
        }
        return;
    }

    if (equals_chars(cmd, L"ls")) {
        files_ls(instance);
        return;
    }

    if (equals_chars(cmd, L"cat")) {
        files_cat(instance, arg);
        return;
    }

    if (equals_chars(cmd, L"exists")) {
        CHAR16 line[96];
        line[0] = 0;
        if (arg[0] == 0) {
            history_append_line(instance, L"exists: missing path");
            return;
        }
        copy_append_text(line, 96, vfs_exists(arg) ? L"yes " : L"no ");
        copy_append_text(line, 96, arg);
        history_append_line(instance, line);
        return;
    }

    if (equals_chars(instance->manifest.id, L"settings")) {
        if (equals_chars(cmd, L"status")) {
            history_append_line(instance, wm_debug_overlay_enabled() ? L"debug overlay: on" : L"debug overlay: off");
            return;
        }

        if (equals_chars(cmd, L"debug")) {
            if (equals_chars(arg, L"on")) {
                wm_set_debug_overlay(TRUE);
                history_append_line(instance, L"debug overlay enabled");
                return;
            }
            if (equals_chars(arg, L"off")) {
                wm_set_debug_overlay(FALSE);
                history_append_line(instance, L"debug overlay disabled");
                return;
            }
            if (equals_chars(arg, L"toggle")) {
                wm_set_debug_overlay(!wm_debug_overlay_enabled());
                history_append_line(instance, wm_debug_overlay_enabled() ? L"debug overlay enabled" : L"debug overlay disabled");
                return;
            }
            history_append_line(instance, L"debug on|off|toggle");
            return;
        }
    }

    history_append_line(instance, L"unknown command");
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

    if (equals_chars(instance->manifest.id, L"shell")) {
        history_append_line(instance, L"Mars shell. Type help.");
        compose_console_view(instance);
    } else if (equals_chars(instance->manifest.id, L"term")) {
        history_append_line(instance, L"Terminal online. Type help.");
        compose_console_view(instance);
    } else if (equals_chars(instance->manifest.id, L"files")) {
        history_append_line(instance, L"File browser ready. Type ls or cat <path>.");
        compose_console_view(instance);
    } else if (equals_chars(instance->manifest.id, L"settings")) {
        history_append_line(instance, L"Settings console. Type status or debug toggle.");
        compose_console_view(instance);
    } else if (equals_chars(instance->manifest.id, L"tasks")) {
        render_tasks_content(instance);
    } else if (equals_chars(instance->manifest.id, L"logs")) {
        render_logs_content(instance);
    } else {
        copy_text(instance->content, L"App online.", APP_CONTENT_CHARS);
        instance->content_len = text_length(instance->content, APP_CONTENT_CHARS);
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
        execute_shell_like(instance);
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

    if (equals_chars(instance->manifest.id, L"tasks") && (instance->ticks % 10) == 0) {
        render_tasks_content(instance);
    } else if (equals_chars(instance->manifest.id, L"logs") && (instance->ticks % 10) == 0) {
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

            if (bounded_text_len(id, 24) > 0) {
                (void)app_launch(id);
            }
        }
        ++processed;
    }

    return TRUE;
}

void app_framework_init(void) {
    g_manifest_count = 0;
    g_instance_count = 0;
    g_app_manager_pid = 0;
    for (UINTN i = 0; i < MAX_APPS; ++i) {
        g_instances[i] = NULL;
    }

    g_app_manager_pid = process_create_kernel(L"app-manager", app_manager_task, NULL, 1, CAP_SYSTEM);
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
        instance->history[0] = 0;
        instance->history_len = 0;
        instance->input_line[0] = 0;
        instance->input_len = 0;
        instance->pid = process_create_kernel(instance->manifest.title, app_task_step, instance, 1, instance->manifest.capabilities);
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