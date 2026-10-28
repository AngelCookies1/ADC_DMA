#include "ring.h"
#include <stdint.h>
#include <string.h>
#include "stm32f1xx_hal.h"

//初始化软件环
void ring_init(ring_t *r)
{
    r->head = 0;
    r->tail = 0;
    r->overflow = 0;
}

//软件环使用的量
uint16_t ring_used(const ring_t *r)
{
    return ((r->head - r->tail) & ring_mask);
}

//软件环的空闲量
uint16_t ring_free(const ring_t *r)
{
    return ((ring_size - 1) - ring_used(r)); //留一个位置来判断空和满
}

//向软件环写数据
uint32_t ring_write_burst(ring_t *r, const uint8_t *src, uint16_t len)
{
    uint16_t head = r->head;
    uint16_t used = ((head - r->tail) & ring_mask);
    uint16_t freeN = ((ring_size - 1) - used);
    uint32_t drop = 0;

    if(len > freeN)
    {
        drop = len - freeN;
        len = freeN;
        r->overflow += drop;
    }

    if(len == 0)
    {
        return drop;
    }

    uint16_t first = ring_size - head;
    if(first > len)
    {
        first = len;
    }
    memcpy(&r->buf[head], src, first);
    if(len > first)
    {
        memcpy(&r->buf[0], src + first, (size_t)(len - first));
    }

    __DMB();  /* 数据先落地，再发布 head */
    r->head = (head + len) & ring_mask;
    return drop;
}

//从软件环读取数据
uint16_t ring_get_burst(ring_t *r, uint8_t *dst, uint16_t len)
{
    uint16_t tail = r->tail;
    uint16_t used = ((r->head - tail) & ring_mask);
    if(len > used)
    {
        len = used;
    }
    if(len == 0)
    {
        return 0;
    }
    uint16_t first = ring_size - tail;
    if(first > len)
    {
        first = len;
    }
    memcpy(dst, &r->buf[tail], first);
    if(first < len)
    {
        memcpy(dst + first, &r->buf[0], (size_t)(len - first));
    }

    __DMB();  /* 数据先落地，再发布 head */
    r->tail = (tail + len) & ring_mask;
    return len;
}

//丢掉指定长度的数据
void ring_drop(ring_t *r, uint16_t len)
{
    uint16_t used = ring_used(r);
    if(len > used)
    {
        len = used;
    }

    __DMB();  /* 数据先落地，再发布 head */
    r->tail = (r->tail + len) & ring_mask;
}

//读一个字节
int ring_getc(ring_t *r)
{
    if(r->head == r->tail)
    {
        return -1; 
    }
    uint8_t c = r->buf[r->tail];
     __DMB();  /* 数据先落地，再发布 head */
    r->tail = (r->tail + 1) & ring_mask;
    return (int)c;
}

//找到指定的字节，idx相对于tail的位置
int ring_find(const ring_t *r, uint8_t ch, uint16_t *idx)
{
    uint16_t used = ring_used(r);
    for(uint16_t i = 0; i < used; i++)
    {
        if(r->buf[(uint16_t)((r->tail + i) & ring_mask)] == ch)
        {
            *idx = i;
            return 1;
        }
    }
    return 0;
}
//拷贝数据
int ring_peek(const ring_t *r, uint16_t offset)
{
    if(offset >= ring_used(r))
    {
        return -1;
    }
    return r->buf[(r->tail + offset) & ring_mask];
}