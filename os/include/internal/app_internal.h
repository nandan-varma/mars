#ifndef APP_INTERNAL_H
#define APP_INTERNAL_H

#include "app.h"

#define MAX_APPS 16
#define APP_EVENTS_PER_STEP 12
#define APP_CONTENT_CHARS 512
#define APP_INPUT_CHARS 96

typedef struct app_instance_t {
    app_manifest_t manifest;
    UINT32 pid;
    UINT32 window_id;
    UINT64 ticks;
    CHAR16 content[APP_CONTENT_CHARS];
    UINTN content_len;
    CHAR16 history[APP_CONTENT_CHARS];
    UINTN history_len;
    CHAR16 input_line[APP_INPUT_CHARS];
    UINTN input_len;
} app_instance_t;

typedef BOOLEAN (*app_task_entry_t)(void *context);
typedef void (*app_content_init_t)(app_instance_t *instance);

void app_registry_init(void);
BOOLEAN app_registry_register(const app_manifest_t *manifest);
UINTN app_registry_count(void);
BOOLEAN app_registry_manifest_at(UINTN index, app_manifest_t *out_manifest);

void app_instance_init(void);
BOOLEAN app_instance_launch(const CHAR16 *id, app_task_entry_t entry, app_content_init_t content_init);

void app_execute_console_command(app_instance_t *instance);

#endif
