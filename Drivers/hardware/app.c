#include "app.h"
#include "ADC.h"
#include "OLED.h"
#include "frame.h"
#include "stm32f1xx_hal.h"
#include <stdint.h>
#include <stdio.h>
#include <sys/_intsup.h>
#include "ring.h"
#include "USART.h"

static sys_state_t g_state = SYS_RUN;
static uint16_t g_period_ms = 100;
static uint32_t g_last_tx = 0;
static uint8_t g_seq = 0;
static uint32_t g_last_oled = 0;

void app_handle(const frame_t *f)
{
    uint8_t rsp[8];
    uint8_t err = 0;
    uint8_t n = 2;

    if(f->len < 1)
    {
        return;
    }

    switch (f->payload[0])
    {
        case CMD_START:
            g_state = SYS_RUN;
            g_last_tx = HAL_GetTick() - g_period_ms;
            rsp[0] = CMD_START;
            rsp[1] = 0;
            break;

        case CMD_STOP:
            g_state = SYS_IDLE;
            rsp[0] = CMD_STOP;
            rsp[1] = 0;
            break;

        case CMD_SET:
            if(f->len < 3)
            {
                err = 1;
            }
            else
            {
                uint16_t p = f->payload[1] | f->payload[2] << 8;
                if(p < 20 || p > 5000)
                {
                    err = 1;
                }
                else
                {
                    g_period_ms = p;
                    g_last_tx = HAL_GetTick() - g_period_ms;
                }
            }
            rsp[0] = CMD_SET;
            rsp[1] = err;
            rsp[2] = g_period_ms & 0xFF;
            rsp[3] = g_period_ms >> 8;
            n = 4;
            break;

        case CMD_GET:
            rsp[0] = CMD_GET;
            rsp[1] = g_state;
            rsp[2] = g_period_ms & 0xFF;
            rsp[3] = g_period_ms >> 8;
            rsp[4] = g_voltage.mv & 0xFF;
            rsp[5] = g_voltage.mv >> 8;
            rsp[6] = (adc_err ? 1 : 0) | (g_rx_ring.overflow ? 2 : 0);
            n = 7;
            break;
        default:
            rsp[0] = f->payload[0];
            rsp[1] = 3;
            break;
    }
    frame_send(f->seq, rsp, n);
}

static void OLED_refresh();

void app_poll()
{
    frame_t f;
    while(frame_try_parse(&g_rx_ring, &f))
    {
        app_handle(&f);
    }
    
    if ((g_state == SYS_RUN) && ((HAL_GetTick() - g_last_tx) >= g_period_ms))
    {
        uint8_t p[7];
        g_last_tx = HAL_GetTick();

        p[0] = CMD_DATA;
        p[1] = (uint8_t)(g_voltage.mv   & 0xFF);
        p[2] = (uint8_t)(g_voltage.mv   >> 8);
        p[3] = (uint8_t)(g_voltage.vmin & 0xFF);
        p[4] = (uint8_t)(g_voltage.vmin >> 8);
        p[5] = (uint8_t)(g_voltage.vmax & 0xFF);
        p[6] = (uint8_t)(g_voltage.vmax >> 8);

        frame_send(g_seq++, p, 7);
    }

    static uint32_t last_oled = 0;
    if(HAL_GetTick() - last_oled >= 200)
    {
        last_oled = HAL_GetTick();

        OLED_clear();
        OLED_str(0, 0, "ADC MONITOR");
        OLED_flush();
    }
    OLED_refresh();
}

static void OLED_refresh()
{
    char s[22];

    if(HAL_GetTick() - g_last_oled < 200)
    {
        return;
    }
    g_last_oled = HAL_GetTick();

    const char *st;
    switch (g_state)
    {
    case SYS_RUN:  st = "RUN "; break;
    case SYS_IDLE: st = "IDLE"; break;
    default:       st = "FLT "; break;
    }
    snprintf(s, sizeof(s), "ST:%s  %3lums", st, (unsigned long)g_period_ms);
    OLED_str(2, 0, s);

    snprintf(s, sizeof(s), "MV:%4ldmv    ", (long)g_voltage.mv);
    OLED_str(3, 0, s);

    snprintf(s, sizeof(s), "MIN:%4ld MAX:%4ld ", (long)g_voltage.vmin, (long)g_voltage.vmax);
    OLED_str(4, 0, s);

    snprintf(s, sizeof(s), "OVF:%lu ERR:%u   ", (unsigned long)g_rx_ring.overflow, (unsigned)adc_err);
    OLED_str(6, 0, s);

    OLED_flush();
}
