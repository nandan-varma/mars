#ifndef EVENT_PACKET_H
#define EVENT_PACKET_H

#include "event_bus.h"

void event_packet_copy(event_packet_t *dst, const event_packet_t *src);
void event_packet_copy_payload(UINT8 *dst, const UINT8 *src, UINTN size, UINTN max_size);

#endif
