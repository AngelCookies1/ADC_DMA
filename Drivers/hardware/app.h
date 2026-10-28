#ifndef __APP_H
#define __APP_H

#include "frame.h"

typedef enum
{
    SYS_IDLE = 0,
    SYS_RUN = 1,
    SYS_FAULT = 2,
} sys_state_t;

void app_handle(const frame_t *f);
void app_poll();

#endif