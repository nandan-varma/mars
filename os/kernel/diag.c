#include "diag.h"

static diag_record_t g_records[DIAG_MAX_RECORDS];
static UINTN g_head;
static UINTN g_count;
static UINT64 g_tick;

void diag_init(void) {
    g_head = 0;
    g_count = 0;
    g_tick = 0;
}

void diag_set_tick(UINT64 tick) {
    g_tick = tick;
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