#include "event_packet.h"

void event_packet_copy(event_packet_t *dst, const event_packet_t *src) {
    if (dst == NULL || src == NULL) {
        return;
    }

    // PERFORMANCE FIX (Issue 2.1): Replace byte-by-byte loop with word-aligned copy
    // event_packet_t is 128 bytes; copying as 16 × 64-bit words = ~16x faster
    // (compiler may use SIMD/memcpy intrinsics for aligned data)
    const UINT64 *src_qwords = (const UINT64 *)src;
    UINT64 *dst_qwords = (UINT64 *)dst;
    for (UINTN i = 0; i < sizeof(event_packet_t) / sizeof(UINT64); ++i) {
        dst_qwords[i] = src_qwords[i];
    }
}

void event_packet_copy_payload(UINT8 *dst, const UINT8 *src, UINTN size, UINTN max_size) {
    if (dst == NULL || src == NULL) {
        return;
    }

    UINTN copy = size;
    if (copy > max_size) {
        copy = max_size;
    }

    // PERFORMANCE FIX: Use word-aligned copy for payload data
    // Most payloads are >= 8 bytes; copy using 64-bit words where possible
    UINTN qword_count = copy / sizeof(UINT64);
    const UINT64 *src_qwords = (const UINT64 *)src;
    UINT64 *dst_qwords = (UINT64 *)dst;
    for (UINTN i = 0; i < qword_count; ++i) {
        dst_qwords[i] = src_qwords[i];
    }
    
    // Copy remaining bytes
    UINTN remaining = copy % sizeof(UINT64);
    UINTN offset = qword_count * sizeof(UINT64);
    for (UINTN i = 0; i < remaining; ++i) {
        dst[offset + i] = src[offset + i];
    }
    
    // Zero-fill rest of max_size
    for (UINTN i = copy; i < max_size; ++i) {
        dst[i] = 0;
    }
}
