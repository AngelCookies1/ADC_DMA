#ifndef __RING_H
#define __RING_H

#include <stdint.h>
#define ring_size 1024
#define ring_mask (ring_size - 1)

typedef struct
{
    uint8_t buf[ring_size]; //存数据的地方
    volatile uint16_t head; //写指针
    volatile uint16_t tail; //读指针
    volatile uint32_t overflow; //累计被丢掉的字节数
} ring_t;

void ring_init(ring_t *r);
uint16_t ring_used(const ring_t *r);
uint16_t ring_free(const ring_t *r);
uint32_t ring_write_burst(ring_t *r, const uint8_t *src, uint16_t len);
uint16_t ring_get_burst(ring_t *r, uint8_t *dst, uint16_t len);
void ring_drop(ring_t *r, uint16_t len);
int ring_getc(ring_t *r);
int ring_find(const ring_t *r, uint8_t ch, uint16_t *idx);
int ring_peek(const ring_t *r, uint16_t offset);

#endif