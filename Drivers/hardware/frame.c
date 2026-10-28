#include "frame.h"
#include "stm32f1xx_hal.h"
#include <stdint.h>
#include <string.h>

extern UART_HandleTypeDef huart1;

static uint8_t tx_buf[FRAME_MAX_PAYLOAD + FRAME_OVERHEAD];
volatile uint8_t tx_busy;

/* 单字节更新 CRC-16/CCITT-FALSE：poly 0x1021，不反射，初值/终值由调用者给 */
static uint16_t crc16_byte(uint16_t crc, uint8_t b)
{
    crc ^= (uint16_t)b << 8;
    for (uint8_t i = 0; i < 8; i++)
    {
        if (crc & 0x8000u)
        {
            crc = (uint16_t)((crc << 1) ^ 0x1021u);   /* 异或，不是与 */
        }
        else
        {
            crc = (uint16_t)(crc << 1);
        }
    }
    return crc;
}

uint16_t crc16_ccitt(uint16_t crc, const uint8_t *data, uint16_t len)
{
    while (len--)
    {
        crc = crc16_byte(crc, *data++);
    }
    return crc;
}

// 发送帧
void frame_send(uint8_t seq, const uint8_t *payload, uint8_t len)
{
    uint16_t n = 0;
    uint16_t crc;

    if (len > FRAME_MAX_PAYLOAD)
    {
        return;
    }
    if(tx_busy == 1)
    {
        return;
    }

    tx_buf[n++] = FRAME_HEAD0;
    tx_buf[n++] = FRAME_HEAD1;
    tx_buf[n++] = len;
    tx_buf[n++] = seq;
    if (len)
    {
        memcpy(&tx_buf[n], payload, len);
        n += (uint16_t)len; //n不含CRC，总长度等于 4 + len
    }

    /* 覆盖范围 = 帧头 4 字节 + 负载 len 字节，正好是 n */
    crc = crc16_ccitt(0xFFFFu, tx_buf, n);
    tx_buf[n++] = (uint8_t)(crc & 0xFFu);
    tx_buf[n++] = (uint8_t)(crc >> 8);

    tx_busy = 1;
    if(HAL_UART_Transmit_DMA(&huart1, tx_buf, n) != HAL_OK)
    {
        tx_busy = 0;
    }
    //HAL_UART_Transmit(&huart1, tx_buf, n, 100);
}

//验证帧
int frame_try_parse(ring_t *r, frame_t *out)
{
    for (;;)
    {
        uint16_t used = ring_used(r);
        uint16_t total;
        uint16_t crc_calc;
        uint16_t crc_recv;
        uint8_t  len;

        /* ① 找帧头，不匹配就丢 1 字节继续找 */
        if (used < 2)
        {
            return 0;
        }
        if (ring_peek(r, 0) != (int)FRAME_HEAD0 || ring_peek(r, 1) != (int)FRAME_HEAD1)
        {
            ring_drop(r, 1);
            continue;
        }

        /* ② 读长度，超过上限说明是假帧头 */
        if (used < 4)
        {
            return 0;
        }
        len = (uint8_t)ring_peek(r, 2);
        if (len > FRAME_MAX_PAYLOAD)
        {
            ring_drop(r, 1);
            continue;                       /* 必须重来，不能拿错误的 len 往下算 */
        }

        /* ③ 整帧到齐才校验 */
        total = (uint16_t)len + FRAME_OVERHEAD;
        if (used < total)
        {
            return 0;                        /* 半帧：等，不要丢 */
        }

        crc_calc = 0xFFFFu;
        for (uint16_t i = 0; i < total - 2u; i++)
        {
            crc_calc = crc16_byte(crc_calc, (uint8_t)ring_peek(r, i)); //不用crc16_ccitt的原因是物理数组是不连续的，所以要一个个去判断
        }
        crc_recv = (uint16_t)(uint8_t)ring_peek(r, total - 2u)
                 | (uint16_t)((uint16_t)(uint8_t)ring_peek(r, total - 1u) << 8);

        if (crc_calc != crc_recv)
        {
            ring_drop(r, 1);                 /* 校验错：只丢帧头，保留可能的真起点 */
            continue;
        }

        /* ④ 校验通过：拷出并消费整帧 */
        out->len = len;
        out->seq = (uint8_t)ring_peek(r, 3);
        for (uint16_t i = 0; i < len; i++)
        {
            out->payload[i] = (uint8_t)ring_peek(r, 4u + i);
        }
        ring_drop(r, total);
        return 1;
    }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if(huart->Instance == USART1)
    {
        tx_busy = 0;
    }
}
