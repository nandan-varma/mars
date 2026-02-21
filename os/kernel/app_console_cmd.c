#include "app_console_cmd.h"

#include "app.h"
#include "diag.h"
#include "heap.h"
#include "app_internal.h"
#include "process.h"
#include "scheduler.h"
#include "timer.h"
#include "vfs.h"
#include "wm.h"

static BOOLEAN app_is_space(CHAR16 ch) {
    return ch == L' ' || ch == L'\t' || ch == L'\r' || ch == L'\n';
}

static BOOLEAN app_equals(const CHAR16 *a, const CHAR16 *b) {
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

static UINTN app_text_length(const CHAR16 *text, UINTN max_chars) {
    if (text == NULL) {
        return 0;
    }

    UINTN len = 0;
    while (len < max_chars && text[len] != 0) {
        ++len;
    }
    return len;
}

static void app_trim_front(CHAR16 *buffer, UINTN max_chars, UINTN *len_io, UINTN needed_space) {
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
        
        // SECURITY FIX #9: Ensure we don't write past buffer bounds
        // If len >= max_chars, we cannot write the null terminator
        if (len >= max_chars) {
            len = max_chars - 1;
        }
        buffer[len] = 0;
    }

    *len_io = len;
}

static void app_append_text(CHAR16 *buffer, UINTN max_chars, UINTN *len_io, const CHAR16 *text) {
    if (buffer == NULL || len_io == NULL || text == NULL || max_chars == 0) {
        return;
    }

    UINTN len = *len_io;
    UINTN add = app_text_length(text, max_chars);
    app_trim_front(buffer, max_chars, &len, add);

    UINTN i = 0;
    while (len + 1 < max_chars && text[i] != 0) {
        buffer[len++] = text[i++];
    }
    
    // SECURITY FIX: Ensure null terminator doesn't overflow
    if (len < max_chars) {
        buffer[len] = 0;
    } else if (max_chars > 0) {
        buffer[max_chars - 1] = 0;
    }
    *len_io = len;
}

static void app_append_char(CHAR16 *buffer, UINTN max_chars, UINTN *len_io, CHAR16 ch) {
    if (buffer == NULL || len_io == NULL || max_chars == 0) {
        return;
    }

    UINTN len = *len_io;
    app_trim_front(buffer, max_chars, &len, 1);
    if (len + 1 < max_chars) {
        buffer[len++] = ch;
        if (len < max_chars) {
            buffer[len] = 0;
        }
    }
    *len_io = len;
}

static void app_copy_text(CHAR16 *dst, const CHAR16 *src, UINTN max_chars) {
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

static void app_append_text_simple(CHAR16 *dst, UINTN max_chars, const CHAR16 *src) {
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

static void app_to_decimal(UINT64 value, CHAR16 *out, UINTN max_chars) {
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

static void app_append_u64(CHAR16 *dst, UINTN max_chars, UINT64 value) {
    CHAR16 tmp[24];
    app_to_decimal(value, tmp, 24);
    app_append_text_simple(dst, max_chars, tmp);
}

static void app_history_append_line(app_instance_t *instance, const CHAR16 *line) {
    if (instance == NULL || line == NULL) {
        return;
    }

    if (instance->history_len > 0) {
        app_append_char(instance->history, APP_CONTENT_CHARS, &instance->history_len, L'\n');
    }
    app_append_text(instance->history, APP_CONTENT_CHARS, &instance->history_len, line);
}

static void files_ls(app_instance_t *instance) {
    if (instance == NULL) {
        return;
    }

    app_history_append_line(instance, L"Entries:");
    const CHAR16 *paths[] = {
        L"/apps/shell.manifest",
        L"/apps/files.manifest",
        L"/apps/term.manifest"
    };

    for (UINTN i = 0; i < (sizeof(paths) / sizeof(paths[0])); ++i) {
        CHAR16 line[96];
        line[0] = 0;
        app_append_text_simple(line, 96, vfs_exists(paths[i]) ? L"- " : L"? ");
        app_append_text_simple(line, 96, paths[i]);
        app_history_append_line(instance, line);
    }
}

static void files_cat(app_instance_t *instance, const CHAR16 *path) {
    if (instance == NULL || path == NULL || path[0] == 0) {
        app_history_append_line(instance, L"cat: missing path");
        return;
    }

    UINT8 bytes[80];
    UINTN read = vfs_read(path, bytes, 79);
    if (read == 0) {
        app_history_append_line(instance, L"cat: file not found");
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
        app_append_char(line, 96, &line_len, ch);
    }
    app_history_append_line(instance, line);
}

typedef BOOLEAN (*cmd_handler_t)(app_instance_t *instance, const CHAR16 *arg);

typedef struct {
    const CHAR16 *name;
    cmd_handler_t handler;
} command_entry_t;

static BOOLEAN cmd_help(app_instance_t *instance, const CHAR16 *arg) {
    (void)arg;
    if (app_equals(instance->manifest.id, L"files")) {
        app_history_append_line(instance, L"help | ls | cat <path> | exists <path> | clear");
    } else if (app_equals(instance->manifest.id, L"settings")) {
        app_history_append_line(instance, L"help | status | debug on|off|toggle | clear");
    } else {
        app_history_append_line(instance, L"help | clear | tick | mem | ps | launch <app>");
        app_history_append_line(instance, L"apps | ls | cat <path> | echo <text>");
    }
    return TRUE;
}

static BOOLEAN cmd_clear(app_instance_t *instance, const CHAR16 *arg) {
    (void)arg;
    instance->history[0] = 0;
    instance->history_len = 0;
    return TRUE;
}

static BOOLEAN cmd_tick(app_instance_t *instance, const CHAR16 *arg) {
    (void)arg;
    CHAR16 line[64];
    app_copy_text(line, L"tick=", 64);
    app_append_u64(line, 64, timer_ticks());
    app_history_append_line(instance, line);
    return TRUE;
}

static BOOLEAN cmd_mem(app_instance_t *instance, const CHAR16 *arg) {
    (void)arg;
    CHAR16 line[96];
    line[0] = 0;
    app_append_text_simple(line, 96, L"heap ");
    app_append_u64(line, 96, heap_used_bytes() / 1024);
    app_append_text_simple(line, 96, L"/");
    app_append_u64(line, 96, heap_total_bytes() / 1024);
    app_append_text_simple(line, 96, L" KB");
    app_history_append_line(instance, line);
    return TRUE;
}

static BOOLEAN cmd_ps(app_instance_t *instance, const CHAR16 *arg) {
    (void)arg;
    CHAR16 line[96];
    line[0] = 0;
    app_append_text_simple(line, 96, L"proc=");
    app_append_u64(line, 96, process_running_count());
    app_append_text_simple(line, 96, L" task=");
    app_append_u64(line, 96, scheduler_task_count());
    app_history_append_line(instance, line);
    return TRUE;
}

static BOOLEAN cmd_apps(app_instance_t *instance, const CHAR16 *arg) {
    (void)arg;
    app_history_append_line(instance, L"shell files term settings tasks logs");
    return TRUE;
}

static BOOLEAN cmd_echo(app_instance_t *instance, const CHAR16 *arg) {
    app_history_append_line(instance, (arg != NULL && arg[0] != 0) ? arg : L"");
    return TRUE;
}

static BOOLEAN cmd_launch(app_instance_t *instance, const CHAR16 *arg) {
    (void)instance;
    if (arg == NULL || arg[0] == 0) {
        app_history_append_line(instance, L"launch: missing app id");
    } else if (app_launch(arg)) {
        app_history_append_line(instance, L"launch: ok");
    } else {
        app_history_append_line(instance, L"launch: failed");
    }
    return TRUE;
}

static BOOLEAN cmd_ls(app_instance_t *instance, const CHAR16 *arg) {
    (void)arg;
    files_ls(instance);
    return TRUE;
}

static BOOLEAN cmd_cat(app_instance_t *instance, const CHAR16 *arg) {
    files_cat(instance, arg);
    return TRUE;
}

static BOOLEAN cmd_exists(app_instance_t *instance, const CHAR16 *arg) {
    CHAR16 line[96];
    line[0] = 0;
    if (arg == NULL || arg[0] == 0) {
        app_history_append_line(instance, L"exists: missing path");
        return TRUE;
    }
    app_append_text_simple(line, 96, vfs_exists(arg) ? L"yes " : L"no ");
    app_append_text_simple(line, 96, arg);
    app_history_append_line(instance, line);
    return TRUE;
}

static BOOLEAN cmd_status(app_instance_t *instance, const CHAR16 *arg) {
    (void)arg;
    if (!app_equals(instance->manifest.id, L"settings")) {
        return FALSE;
    }

    app_history_append_line(instance, wm_debug_overlay_enabled() ? L"debug overlay: on" : L"debug overlay: off");
    return TRUE;
}

static BOOLEAN cmd_debug(app_instance_t *instance, const CHAR16 *arg) {
    if (!app_equals(instance->manifest.id, L"settings")) {
        return FALSE;
    }

    if (app_equals(arg, L"on")) {
        wm_set_debug_overlay(TRUE);
        app_history_append_line(instance, L"debug overlay enabled");
        return TRUE;
    }
    if (app_equals(arg, L"off")) {
        wm_set_debug_overlay(FALSE);
        app_history_append_line(instance, L"debug overlay disabled");
        return TRUE;
    }
    if (app_equals(arg, L"toggle")) {
        wm_set_debug_overlay(!wm_debug_overlay_enabled());
        app_history_append_line(instance, wm_debug_overlay_enabled() ? L"debug overlay enabled" : L"debug overlay disabled");
        return TRUE;
    }

    app_history_append_line(instance, L"debug on|off|toggle");
    return TRUE;
}

void app_execute_console_command(app_instance_t *instance) {
    if (instance == NULL) {
        return;
    }

    CHAR16 cmd[24];
    CHAR16 arg[64];
    app_parse_command(instance->input_line, cmd, 24, arg, 64);
    if (cmd[0] == 0) {
        return;
    }

    static const command_entry_t table[] = {
        { L"help", cmd_help },
        { L"clear", cmd_clear },
        { L"tick", cmd_tick },
        { L"mem", cmd_mem },
        { L"ps", cmd_ps },
        { L"apps", cmd_apps },
        { L"echo", cmd_echo },
        { L"launch", cmd_launch },
        { L"open", cmd_launch },
        { L"ls", cmd_ls },
        { L"cat", cmd_cat },
        { L"exists", cmd_exists },
        { L"status", cmd_status },
        { L"debug", cmd_debug }
    };

    for (UINTN i = 0; i < (sizeof(table) / sizeof(table[0])); ++i) {
        if (!app_equals(cmd, table[i].name)) {
            continue;
        }

        if (table[i].handler(instance, arg)) {
            return;
        }
    }

    app_history_append_line(instance, L"unknown command");
}

void app_parse_command(const CHAR16 *input, CHAR16 *cmd, UINTN cmd_max, CHAR16 *arg, UINTN arg_max) {
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
    while (input[i] != 0 && app_is_space(input[i])) {
        ++i;
    }

    UINTN c = 0;
    while (input[i] != 0 && !app_is_space(input[i]) && c + 1 < cmd_max) {
        cmd[c++] = input[i++];
    }
    cmd[c] = 0;

    while (input[i] != 0 && app_is_space(input[i])) {
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
