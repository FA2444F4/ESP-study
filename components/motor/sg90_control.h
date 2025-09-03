#ifndef SG90_CONTROL_H
#define SG90_CONTROL_H
#include "cmd_defs.h"

void sg90_control_init();
void sg90_cmd_handler(const char *command, const char *args,cmd_responder_t responder, void *context);

#endif SG90_CONTROL_H