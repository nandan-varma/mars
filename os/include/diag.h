#ifndef DIAG_H
#define DIAG_H

#include "uefi.h"

#define DIAG_MAX_RECORDS 256

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

void diag_init(void);
void diag_set_tick(UINT64 tick);
void diag_log(UINT32 domain, UINT32 code, UINT64 a, UINT64 b);
diag_snapshot_t diag_snapshot(void);
BOOLEAN diag_latest(diag_record_t *out_record);
void diag_capture_crash(UINTN vector, UINT64 code, UINT64 address);

#endif