#include "diag.h"
#include "serial.h"

static diag_record_t g_records[DIAG_MAX_RECORDS];
static UINTN g_head;
static UINTN g_count;
static UINT64 g_tick;
static UINT32 g_stage;

static diag_stage_event_t g_stages[DIAG_STAGE_HISTORY];
static UINTN g_stage_head;
static UINTN g_stage_count;

static BOOLEAN g_serial_drain = TRUE;

static void drain_record(const diag_record_t *r) {
    serial_write_str("[d ");
    serial_write_dec(r->tick);
    serial_write_str(" dom=");
    serial_write_hex32(r->domain);
    serial_write_str(" code=");
    serial_write_hex32(r->code);
    serial_write_str(" a=");
    serial_write_hex64(r->a);
    serial_write_str(" b=");
    serial_write_hex64(r->b);
    serial_write_str("]\r\n");
}

static void drain_stage(UINT64 tick, UINT32 stage) {
    serial_write_str("[stage ");
    serial_write_dec(stage);
    serial_write_str(" tick=");
    serial_write_dec(tick);
    serial_write_str("]\r\n");
}

void diag_init(void) {
    g_head = 0;
    g_count = 0;
    g_tick = 0;
    g_stage = 0;
    g_stage_head = 0;
    g_stage_count = 0;
    g_serial_drain = TRUE;
}

void diag_set_tick(UINT64 tick) {
    g_tick = tick;
}

void diag_set_stage(UINT32 stage) {
    g_stage = stage;
    g_stages[g_stage_head].tick = g_tick;
    g_stages[g_stage_head].stage = stage;
    g_stage_head = (g_stage_head + 1) % DIAG_STAGE_HISTORY;
    if (g_stage_count < DIAG_STAGE_HISTORY) {
        ++g_stage_count;
    }
    if (g_serial_drain) {
        drain_stage(g_tick, stage);
    }
}

UINT32 diag_stage(void) {
    return g_stage;
}

void diag_log(UINT32 domain, UINT32 code, UINT64 a, UINT64 b) {
    UINTN slot = g_head;
    g_records[slot].tick = g_tick;
    g_records[slot].domain = domain;
    g_records[slot].code = code;
    g_records[slot].a = a;
    g_records[slot].b = b;

    g_head = (g_head + 1) % DIAG_MAX_RECORDS;
    if (g_count < DIAG_MAX_RECORDS) {
        ++g_count;
    }

    if (g_serial_drain) {
        drain_record(&g_records[slot]);
    }
}

diag_snapshot_t diag_snapshot(void) {
    diag_snapshot_t snapshot;
    snapshot.count = g_count;
    snapshot.records = g_records;
    return snapshot;
}

BOOLEAN diag_latest(diag_record_t *out_record) {
    if (out_record == NULL || g_count == 0) {
        return FALSE;
    }

    UINTN latest = (g_head == 0) ? (DIAG_MAX_RECORDS - 1) : (g_head - 1);
    *out_record = g_records[latest];
    return TRUE;
}

BOOLEAN diag_recent(UINTN offset_from_latest, diag_record_t *out_record) {
    if (out_record == NULL || g_count == 0 || offset_from_latest >= g_count) {
        return FALSE;
    }

    UINTN latest = (g_head == 0) ? (DIAG_MAX_RECORDS - 1) : (g_head - 1);
    UINTN index = (latest + DIAG_MAX_RECORDS - (offset_from_latest % DIAG_MAX_RECORDS)) % DIAG_MAX_RECORDS;
    *out_record = g_records[index];
    return TRUE;
}

void diag_capture_crash(UINTN vector, UINT64 code, UINT64 address) {
    diag_log(0xDEADU, (UINT32)vector, code, address);
}

void diag_set_serial_drain(BOOLEAN enabled) {
    g_serial_drain = enabled;
}

BOOLEAN diag_serial_drain_enabled(void) {
    return g_serial_drain;
}

UINTN diag_stage_history_count(void) {
    return g_stage_count;
}

BOOLEAN diag_stage_history_entry(UINTN index_from_oldest, diag_stage_event_t *out) {
    if (out == NULL || index_from_oldest >= g_stage_count) {
        return FALSE;
    }
    UINTN oldest = (g_stage_count < DIAG_STAGE_HISTORY)
        ? 0
        : g_stage_head;
    UINTN slot = (oldest + index_from_oldest) % DIAG_STAGE_HISTORY;
    *out = g_stages[slot];
    return TRUE;
}

void diag_dump_stage_history_to_serial(void) {
    serial_write_str("-- diag stage history (oldest first) --\r\n");
    for (UINTN i = 0; i < g_stage_count; ++i) {
        diag_stage_event_t ev;
        if (diag_stage_history_entry(i, &ev)) {
            drain_stage(ev.tick, ev.stage);
        }
    }
}

void diag_dump_records_to_serial(UINTN max_records) {
    UINTN n = (max_records < g_count) ? max_records : g_count;
    serial_write_str("-- diag records (newest first, ");
    serial_write_dec(n);
    serial_write_str(") --\r\n");
    for (UINTN i = 0; i < n; ++i) {
        diag_record_t r;
        if (diag_recent(i, &r)) {
            drain_record(&r);
        }
    }
}
