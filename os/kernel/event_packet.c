#include "event_packet.h"

void event_packet_copy(event_packet_t *dst, const event_packet_t *src) {
    if (dst == NULL || src == NULL) {
        return;
    }

    const UINT8 *src_bytes = (const UINT8 *)src;
    UINT8 *dst_bytes = (UINT8 *)dst;
    for (UINTN i = 0; i < sizeof(event_packet_t); ++i) {
        dst_bytes[i] = src_bytes[i];
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
