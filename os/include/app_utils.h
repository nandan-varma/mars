#pragma once

#include <uefi.h>

void app_utils_copy_chars(CHAR16 *dst, const CHAR16 *src, UINTN max_chars);
BOOLEAN app_utils_equals(const CHAR16 *a, const CHAR16 *b);
UINTN app_utils_text_length(const CHAR16 *text, UINTN max_chars);
void app_utils_buffer_trim_front(CHAR16 *buffer, UINTN max_chars, UINTN *len_io, UINTN needed_space);
void app_utils_buffer_append_text(CHAR16 *buffer, UINTN max_chars, UINTN *len_io, const CHAR16 *text);
void app_utils_buffer_append_char(CHAR16 *buffer, UINTN max_chars, UINTN *len_io, CHAR16 ch);
void app_utils_copy_text(CHAR16 *dst, const CHAR16 *src, UINTN max_chars);
void app_utils_copy_append(CHAR16 *dst, UINTN max_chars, const CHAR16 *src);
void app_utils_to_decimal(UINT64 value, CHAR16 *out, UINTN max_chars);
void app_utils_append_u64(CHAR16 *dst, UINTN max_chars, UINT64 value);
void app_utils_to_hex32(UINT32 value, CHAR16 *out, UINTN max_chars);
UINTN app_utils_bounded_len(const CHAR16 *text, UINTN max_chars);