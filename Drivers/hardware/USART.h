#ifndef __USART_H
#define __USART_H

#include "ring.h"
#include "stm32f1xx.h"

#define RX_DMA_SIZE  256
#define RX_DMA_END   RX_DMA_SIZE

extern ring_t g_rx_ring;

int _write(int file, char *ptr, int len);
void usart_rx_arm(void);
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t pos);
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart);

#endif