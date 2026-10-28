#include "ADC.h"
#include "stm32f103xb.h"
#include "stm32f1xx_hal.h"
#include <stdint.h>

static uint16_t g_ma[MA_WIN];
static uint8_t g_ma_idx = 0;
static uint16_t ma_push(uint16_t v);

volatile uint16_t adc_dma_buff[ADC_BUFF_LEN];
volatile uint8_t  adc_half_ready = 0;
volatile uint8_t adc_err;

uint16_t adc_frame_a[ADC_FRAME_LEN];
uint16_t adc_frame_b[ADC_FRAME_LEN];

voltage g_voltage;

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef* hadc)
{
    if(hadc->Instance == ADC1)
    {
        adc_half_ready = 1;
    }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    if(hadc->Instance == ADC1)
    {
        adc_half_ready = 2;
    }
}

void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc)
{
    if(hadc->Instance == ADC1)
    {
        adc_err = 1;
    }
}

uint16_t ma_push(uint16_t v)
{
    g_ma[g_ma_idx] = v;
    g_ma_idx = (g_ma_idx + 1) % MA_WIN;
    uint32_t s = 0;
    for(int i = 0; i < MA_WIN; i++)
    {
        s += g_ma[i];
    }
    return (s / MA_WIN);
}

void process_frame(const uint16_t *frame, uint16_t len)
{
    uint32_t sum;
    uint16_t vmin, vmax, mean;

    if(len == 0)
    {
        return;
    }

    vmin = frame[0];
    vmax = frame[0];
    sum = 0;

    for(uint16_t  i = 0; i < len; i++)
    {
        uint16_t v = frame[i];
        sum += v;
        if(v < vmin)
        {
            vmin = v;
        }
        if(v > vmax)
        {
            vmax = v;
        }
    }
    mean = (uint16_t)(sum / len);

    g_voltage.vmin = vmin * 3300 / 4095;
    g_voltage.vmax = vmax * 3300 / 4095;
    g_voltage.v_mean = ma_push(mean);
    g_voltage.mv = g_voltage.v_mean * 3300 / 4095;
}
