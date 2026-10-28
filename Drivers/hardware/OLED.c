#include "OLED.h"
#include <stdint.h>
#include <string.h>
#include <sys/_intsup.h>
#include "stm32f1xx.h"
#include "OLED_font.h"

#define OLED_ARR (0x3C << 1) //I2C 接口用的是 8 位形式（含读写位），所以要左移一位把 7 位地址变成 8 位。

extern I2C_HandleTypeDef hi2c1;
static uint8_t fb[8][128];

static const uint8_t init_seq[] = 
{
    0xAE,              /* 关显示 */
    0xD5, 0x80,        /* 时钟分频 */
    0xA8, 0x3F,        /* 复用率 64 */
    0xD3, 0x00,        /* 显示偏移 0 */
    0x40,              /* 起始行 0 */
    0x8D, 0x14,        /* 电荷泵开 */
    0x20, 0x00,        /* 水平寻址模式 */
    0xA1,              /* 段重映射 */
    0xC8,              /* COM 反向扫描 */
    0xDA, 0x12,        /* COM 引脚配置 */
    0x81, 0xCF,        /* 对比度 */
    0xD9, 0xF1,        /* 预充电 */
    0xDB, 0x40,        /* VCOMH */
    0xA4,              /* 显示跟随显存 */
    0xA6,              /* 正常（非反色） */
    0x2E,              /* 关滚动 */
    0xAF,              /* 开显示 */
};

static void cmd(uint8_t c)
{
    HAL_I2C_Mem_Write(&hi2c1, OLED_ARR, 0x00, I2C_MEMADD_SIZE_8BIT, &c, 1, 100);
}

static void cmds(const uint8_t *c, uint16_t n)
{
    HAL_I2C_Mem_Write(&hi2c1, OLED_ARR, 0x00, I2C_MEMADD_SIZE_8BIT, (uint8_t *)c, n, 100);
}

static void push(const uint8_t *d, uint16_t n)
{
    HAL_I2C_Mem_Write(&hi2c1, OLED_ARR, 0x40, I2C_MEMADD_SIZE_8BIT, (uint8_t *)d, n, 100);
}

void OLED_init()
{
    cmds(init_seq, sizeof(init_seq));
    OLED_clear();
    OLED_flush();
}

void OLED_clear()
{
    memset(fb, 0, sizeof(fb));
}

void OLED_flush(void)
{
    cmd(0x21); cmd(0x00); cmd(0x7F);   /* 列 0~127 */
    cmd(0x22); cmd(0x00); cmd(0x07);   /* 页 0~7  */
    push(&fb[0][0], 1024);
}

void OLED_char(uint8_t page, uint8_t col, char ch)
{
    if(ch < ' ' || ch > '~')
    {
        ch = ' ';
    }
    const uint8_t *p = font_6x8[ch - ' '];
    for(uint8_t i = 0; i < 6; i++)
    {
        if(col + i < 128)
        {
            fb[page][col + i] = p[i];
        }
    }
}

void OLED_str(uint8_t page, uint8_t col, const char *s)
{
    while (*s && col < 127)
    {
        OLED_char(page, col, *s++);
        col += 6;
    }
}