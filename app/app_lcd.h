#ifndef __APP_LCD_H
#define __APP_LCD_H

#include "gui.h"
#include "pic.h"
#include <stdint.h>

#ifndef delay_ms
#define delay_ms(ms) HAL_Delay(ms)
#endif

#ifndef BRED
#define BRED 0xF81F
#endif

extern u16 ColorTab[5];

void DrawTestPage(u8 *str);
void main_test(void);
void Test_Color(void);
void Test_FillRec(void);
void Test_Circle(void);
void English_Font_test(void);
void Chinese_Font_test(void);
void Pic_test(void);

#endif
