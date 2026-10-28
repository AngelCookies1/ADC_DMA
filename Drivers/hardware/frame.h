#ifndef __FRAME_H
#define __FRAME_H

#include <stdint.h>
#include "ring.h"

#define FRAME_HEAD0         0xAA
#define FRAME_HEAD1         0x55
#define FRAME_MAX_PAYLOAD   64u
#define FRAME_OVERHEAD      6u    //固定开销，6 字节（帧头+长度+序号+CRC）
/* 帧格式: AA 55 LEN SEQ CRC_L CRC_H */

#define CMD_PING            0x01u
#define CMD_ECHO            0x02u
#define CMD_VERSION         0X03U

#define CMD_SET             0x10
#define CMD_GET             0x11
#define CMD_START           0x12
#define CMD_STOP            0x13
#define CMD_DATA            0x20

typedef struct
{
    uint8_t len;
    uint8_t seq; //序号，请求/应答相同，便于配对
    uint8_t payload[FRAME_MAX_PAYLOAD]; //命令字 + 数据
} frame_t;

uint16_t crc16_ccitt(uint16_t crc, const uint8_t *data, uint16_t len);
void frame_send(uint8_t seq, const uint8_t *payload, uint8_t len);
int frame_try_parse(ring_t *r, frame_t *out);

extern volatile uint8_t tx_busy;


#endif