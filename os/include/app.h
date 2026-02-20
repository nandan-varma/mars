#ifndef APP_H
#define APP_H

#include "uefi.h"

typedef struct {
    CHAR16 id[24];
    CHAR16 title[32];
    UINT32 capabilities;
    INT32 start_x;
    INT32 start_y;
    INT32 width;
    INT32 height;
} app_manifest_t;

void app_framework_init(void);
BOOLEAN app_register(const app_manifest_t *manifest);
BOOLEAN app_launch(const CHAR16 *id);
void app_launch_core_suite(void);

#endif