#ifndef APP_CONSOLE_CMD_H
#define APP_CONSOLE_CMD_H

#include "uefi.h"

struct app_instance_t;

void app_parse_command(const CHAR16 *input, CHAR16 *cmd, UINTN cmd_max, CHAR16 *arg, UINTN arg_max);
void app_execute_console_command(struct app_instance_t *instance);

#endif
