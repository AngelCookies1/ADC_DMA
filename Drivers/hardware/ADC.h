#ifndef __ADC_H
#define __ADC_H

#include <stdint.h>

#define ADC_FRAME_LEN 512
#define ADC_BUFF_LEN      ADC_FRAME_LEN * 2

#define MA_WIN  8

typedef struct
{
  uint16_t raw_min;
  uint16_t raw_max;
} raw;

typedef struct
{
  uint16_t vmin;
  uint16_t vmax;
  uint16_t v_mean;
  uint32_t mv; //毫伏
} voltage;

extern volatile uint16_t adc_dma_buff[ADC_BUFF_LEN];
extern volatile uint8_t adc_half_ready;
extern volatile uint8_t adc_err;

extern uint16_t adc_frame_a[ADC_FRAME_LEN];
extern uint16_t adc_frame_b[ADC_FRAME_LEN];

extern voltage g_voltage;

void process_frame(const uint16_t *frame, uint16_t len);


#endif