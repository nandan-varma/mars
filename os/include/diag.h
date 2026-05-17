#ifndef DIAG_H
#define DIAG_H

#include "uefi.h"

#define DIAG_MAX_RECORDS 256
#define DIAG_STAGE_HISTORY 32

typedef struct {
    UINT64 tick;
    UINT32 domain;
    UINT32 code;
    UINT64 a;
    UINT64 b;
} diag_record_t;

typedef struct {
    UINTN count;
    const diag_record_t *records;
} diag_snapshot_t;

typedef struct {
    UINT64 tick;
    UINT32 stage;
} diag_stage_event_t;

void diag_init(void);
void diag_set_tick(UINT64 tick);
void diag_set_stage(UINT32 stage);
UINT32 diag_stage(void);
void diag_log(UINT32 domain, UINT32 code, UINT64 a, UINT64 b);
diag_snapshot_t diag_snapshot(void);
BOOLEAN diag_latest(diag_record_t *out_record);
BOOLEAN diag_recent(UINTN offset_from_latest, diag_record_t *out_record);
void diag_capture_crash(UINTN vector, UINT64 code, UINT64 address);

// Toggle the per-call serial drain. ON by default; turn off only if
// something downstream (e.g. a wedged UART on real HW) makes serial too
// expensive in a hot path.
void diag_set_serial_drain(BOOLEAN enabled);
BOOLEAN diag_serial_drain_enabled(void);

// Stage history accessors — used by panic to print the boot path that led
// to the fault. count is the number of valid entries (<= DIAG_STAGE_HISTORY).
UINTN diag_stage_history_count(void);
BOOLEAN diag_stage_history_entry(UINTN index_from_oldest, diag_stage_event_t *out);

// Bulk dump helpers: write every kept record / stage event to serial.
// Safe to call from panic context.
void diag_dump_stage_history_to_serial(void);
void diag_dump_records_to_serial(UINTN max_records);

#endif
