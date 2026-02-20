#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "uefi.h"
#include "boot_info.h"

typedef struct {
    UINT32 *base;
    UINTN size;
    UINT32 width;
    UINT32 height;
    UINT32 pitch;
} framebuffer_t;

void framebuffer_init(const boot_info_t *boot_info);
const framebuffer_t *framebuffer_get(void);

void drawPixel(INT32 x, INT32 y, UINT32 color);
void drawRect(INT32 x, INT32 y, INT32 width, INT32 height, UINT32 color);
void clearScreen(UINT32 color);
void framebuffer_present(void);

void drawChar(INT32 x, INT32 y, CHAR16 c, UINT32 fg, UINT32 bg);
void drawString(INT32 x, INT32 y, const CHAR16 *text, UINT32 fg, UINT32 bg);

#endif
