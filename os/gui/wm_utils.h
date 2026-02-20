#ifndef WM_UTILS_H
#define WM_UTILS_H

#include "uefi.h"

UINTN wm_text_len16(const CHAR16 *text, UINTN max_chars);
void wm_append_text(CHAR16 *dst, UINTN max_chars, const CHAR16 *src);
void wm_to_decimal(UINT64 value, CHAR16 *out, UINTN max_chars);
INT32 wm_clamp_i32(INT32 value, INT32 low, INT32 high);
BOOLEAN wm_point_in_rect(INT32 x, INT32 y, INT32 rx, INT32 ry, INT32 rw, INT32 rh);

#endif
