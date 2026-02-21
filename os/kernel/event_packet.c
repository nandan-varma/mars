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

    UINTN i = 0;
    for (; i < copy; ++i) {
        dst[i] = src[i];
    }
    for (; i < max_size; ++i) {
        dst[i] = 0;
    }
}
