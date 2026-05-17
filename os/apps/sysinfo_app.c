#include "uefi.h"
#include "app.h"
#include "input.h"
#include "process.h"
#include "scheduler.h"
#include "timer.h"
#include "memory.h"
#include "heap.h"
#include "wm.h"
#include "internal/app_internal.h"

typedef struct {
    CHAR16 content[512];
    UINT32 content_len;
    UINTN tick_count;
} sysinfo_app_state_t;

static sysinfo_app_state_t *g_sysinfo_state = NULL;

static void update_sysinfo(void) {
    if (g_sysinfo_state == NULL) {
        return;
    }

    UINTN idx = 0;

    const CHAR16 *title = L"System Information\n";
    for (UINTN i = 0; title[i] != 0 && idx < 511; ++i) {
        g_sysinfo_state->content[idx++] = title[i];
    }
    g_sysinfo_state->content[idx++] = L'=';
    g_sysinfo_state->content[idx++] = L'\n';
    g_sysinfo_state->content[idx++] = L'\n';

    BOOLEAN preemptive = scheduler_timer_preemptive();
    const CHAR16 *sched = preemptive ? L"Scheduler: Preemptive" : L"Scheduler: Cooperative";
    for (UINTN i = 0; sched[i] != 0 && idx < 511; ++i) {
        g_sysinfo_state->content[idx++] = sched[i];
    }
    g_sysinfo_state->content[idx++] = L'\n';

    UINT32 task_count = scheduler_task_count();
    const CHAR16 *task_str = L"Tasks: ";
    for (UINTN i = 0; task_str[i] != 0 && idx < 511; ++i) {
        g_sysinfo_state->content[idx++] = task_str[i];
    }
    UINT32 tasks = task_count;
    CHAR16 task_buf[8];
    UINTN task_idx = 0;
    if (tasks == 0) {
        task_buf[task_idx++] = L'0';
    } else {
        CHAR16 rev[8];
        UINTN rev_idx = 0;
        while (tasks > 0 && rev_idx < (sizeof(rev) / sizeof(rev[0]))) {
            rev[rev_idx++] = L'0' + (tasks % 10);
            tasks /= 10;
        }
        while (rev_idx > 0) {
            task_buf[task_idx++] = rev[--rev_idx];
        }
    }
    task_buf[task_idx] = 0;
    for (UINTN i = 0; task_buf[i] != 0 && idx < 511; ++i) {
        g_sysinfo_state->content[idx++] = task_buf[i];
    }
    g_sysinfo_state->content[idx++] = L'\n';

    UINTN procs = process_count();
    const CHAR16 *proc_str = L"Processes: ";
    for (UINTN i = 0; proc_str[i] != 0 && idx < 511; ++i) {
        g_sysinfo_state->content[idx++] = proc_str[i];
    }
    CHAR16 proc_buf[8];
    UINTN proc_idx = 0;
    if (procs == 0) {
        proc_buf[proc_idx++] = L'0';
    } else {
        CHAR16 rev[8];
        UINTN rev_idx = 0;
        while (procs > 0 && rev_idx < (sizeof(rev) / sizeof(rev[0]))) {
            rev[rev_idx++] = L'0' + (procs % 10);
            procs /= 10;
        }
        while (rev_idx > 0) {
            proc_buf[proc_idx++] = rev[--rev_idx];
        }
    }
    proc_buf[proc_idx] = 0;
    for (UINTN i = 0; proc_buf[i] != 0 && idx < 511; ++i) {
        g_sysinfo_state->content[idx++] = proc_buf[i];
    }
    g_sysinfo_state->content[idx++] = L'\n';

    UINTN mem_used = heap_used_bytes();
    UINTN mem_total = heap_total_bytes();
    const CHAR16 *mem_str = L"Heap: ";
    for (UINTN i = 0; mem_str[i] != 0 && idx < 511; ++i) {
        g_sysinfo_state->content[idx++] = mem_str[i];
    }
    UINT32 mem_kb = (UINT32)(mem_used / 1024);
    UINT32 total_kb = (UINT32)(mem_total / 1024);
    CHAR16 mem_buf[24];
    UINTN mem_idx = 0;
    UINT32 mem = mem_kb;
    if (mem == 0) {
        mem_buf[mem_idx++] = L'0';
    } else {
        CHAR16 rev[12];
        UINTN rev_idx = 0;
        while (mem > 0 && rev_idx < (sizeof(rev) / sizeof(rev[0]))) {
            rev[rev_idx++] = L'0' + (mem % 10);
            mem /= 10;
        }
        while (rev_idx > 0) {
            mem_buf[mem_idx++] = rev[--rev_idx];
        }
    }
    mem_buf[mem_idx++] = L'/';
    mem = total_kb;
    if (mem == 0) {
        mem_buf[mem_idx++] = L'0';
    } else {
        CHAR16 rev[12];
        UINTN rev_idx = 0;
        while (mem > 0 && rev_idx < (sizeof(rev) / sizeof(rev[0]))) {
            rev[rev_idx++] = L'0' + (mem % 10);
            mem /= 10;
        }
        while (rev_idx > 0) {
            mem_buf[mem_idx++] = rev[--rev_idx];
        }
    }
    mem_buf[mem_idx++] = L' ';
    mem_buf[mem_idx++] = L'K';
    mem_buf[mem_idx++] = L'B';
    mem_buf[mem_idx] = 0;
    for (UINTN i = 0; mem_buf[i] != 0 && idx < 511; ++i) {
        g_sysinfo_state->content[idx++] = mem_buf[i];
    }
    g_sysinfo_state->content[idx++] = L'\n';

    UINT64 ticks = timer_ticks();
    UINT64 uptime = ticks / timer_hz();
    const CHAR16 *up_str = L"Uptime: ";
    for (UINTN i = 0; up_str[i] != 0 && idx < 511; ++i) {
        g_sysinfo_state->content[idx++] = up_str[i];
    }
    UINT32 up = (UINT32)uptime;
    CHAR16 up_buf[16];
    UINTN up_idx = 0;
    if (up == 0) {
        up_buf[up_idx++] = L'0';
    } else {
        CHAR16 rev[16];
        UINTN rev_idx = 0;
        while (up > 0 && rev_idx < (sizeof(rev) / sizeof(rev[0]))) {
            rev[rev_idx++] = L'0' + (up % 10);
            up /= 10;
        }
        while (rev_idx > 0) {
            up_buf[up_idx++] = rev[--rev_idx];
        }
    }
    up_buf[up_idx++] = L' ';
    up_buf[up_idx++] = L's';
    up_buf[up_idx++] = L'e';
    up_buf[up_idx++] = L'c';
    up_buf[up_idx] = 0;
    for (UINTN i = 0; up_buf[i] != 0 && idx < 511; ++i) {
        g_sysinfo_state->content[idx++] = up_buf[i];
    }
    g_sysinfo_state->content[idx++] = L'\n';

    const CHAR16 *hz_str = L"HZ: ";
    for (UINTN i = 0; hz_str[i] != 0 && idx < 511; ++i) {
        g_sysinfo_state->content[idx++] = hz_str[i];
    }
    UINT32 hz = timer_hz();
    CHAR16 hz_buf[16];
    UINTN hz_idx = 0;
    if (hz == 0) {
        hz_buf[hz_idx++] = L'0';
    } else {
        CHAR16 rev[16];
        UINTN rev_idx = 0;
        while (hz > 0 && rev_idx < (sizeof(rev) / sizeof(rev[0]))) {
            rev[rev_idx++] = L'0' + (hz % 10);
            hz /= 10;
        }
        while (rev_idx > 0) {
            hz_buf[hz_idx++] = rev[--rev_idx];
        }
    }
    hz_buf[hz_idx] = 0;
    for (UINTN i = 0; hz_buf[i] != 0 && idx < 511; ++i) {
        g_sysinfo_state->content[idx++] = hz_buf[i];
    }

    g_sysinfo_state->content_len = idx;
}

BOOLEAN sysinfo_app_init(UINT32 window_id) {
    (void)window_id;
    g_sysinfo_state = heap_alloc(sizeof(sysinfo_app_state_t));
    if (g_sysinfo_state == NULL) {
        return FALSE;
    }

    g_sysinfo_state->content[0] = 0;
    g_sysinfo_state->content_len = 0;
    g_sysinfo_state->tick_count = 0;

    update_sysinfo();

    return TRUE;
}

void sysinfo_app_render(app_instance_t *instance) {
    if (g_sysinfo_state == NULL) {
        return;
    }

    g_sysinfo_state->tick_count++;

    if (g_sysinfo_state->tick_count % 30 == 0) {
        update_sysinfo();
    }

    // Copy content to instance
    UINTN len = g_sysinfo_state->content_len;
    if (len > APP_CONTENT_CHARS) len = APP_CONTENT_CHARS;
    for (UINTN i = 0; i < len; ++i) {
        instance->content[i] = g_sysinfo_state->content[i];
    }
    instance->content[len] = 0;
    instance->content_len = (UINT32)len;
}

void sysinfo_app_handle_input(const input_event_t *event) {
    if (g_sysinfo_state == NULL || event == NULL) {
        return;
    }

    if (event->type == INPUT_EVENT_KEY_DOWN) {
        if (event->data.key.unicode == L'r' || event->data.key.unicode == L'R') {
            update_sysinfo();
        }
    }
}

extern void sysinfo_app_register(void) {
    app_manifest_t manifest = {
        L"sysinfo_app",
        L"System Info",
        CAP_INPUT,
        550, 250,
        380, 280
    };
    app_register(&manifest);
}
