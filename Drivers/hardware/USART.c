#include "USART.h"
#include "ring.h"
#include "stm32f1xx_hal.h"
#include <stdint.h>

extern UART_HandleTypeDef huart1;
extern DMA_HandleTypeDef hdma_usart1_rx;

static uint8_t g_rx_dma[RX_DMA_SIZE];
static uint16_t g_rx_last;
ring_t g_rx_ring;

void usart_rx_arm(void)
{
    g_rx_last = 0;
    HAL_UARTEx_ReceiveToIdle_DMA(&huart1, g_rx_dma, RX_DMA_SIZE);
    __HAL_DMA_DISABLE_IT(&hdma_usart1_rx, DMA_IT_HT); //不需要半传输中断
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t pos)
{
    if(huart->Instance != USART1)
    {
        return;
    }
    if(pos == g_rx_last)
    {
        return;
    }
    uint16_t end;
    if(pos > g_rx_last)
    {
        end = pos;
    }
    else
    {
        end = RX_DMA_END;
    }

    ring_write_burst(&g_rx_ring, &g_rx_dma[g_rx_last], end - g_rx_last);

    if(pos < g_rx_last)
    {
        ring_write_burst(&g_rx_ring, &g_rx_dma[0], pos);
    }
    if(pos == RX_DMA_END)
    {
        g_rx_last = 0;
    }
    else
    {
        g_rx_last = pos;
    }
}

int _write(int file, char *ptr, int len)
{
    (void)file;
    HAL_UART_Transmit(&huart1, (uint8_t *)ptr, len, HAL_MAX_DELAY);
    return len;
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if(huart->Instance == USART1)
    {
        usart_rx_arm();
    }
}
