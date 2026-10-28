#ifndef __OLED_H
#define __OLED_H

#include "stdint.h"

void OLED_init();
void OLED_clear();
void OLED_flush(void);
void OLED_char(uint8_t page, uint8_t col, char ch);
void OLED_str(uint8_t page, uint8_t col, const char *s);

#endif