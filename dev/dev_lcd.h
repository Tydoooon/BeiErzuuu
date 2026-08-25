#ifndef __DEV_LCD_H
#define __DEV_LCD_H

#include "bsp_lcd.h"
#include "pic.h"

// 将 LCD 读取到的 BGR/GBR 颜色值转换为 RGB565 颜色值
u16 LCD_BGR2RGB(u16 c);

// 使用 Bresenham 算法绘制空心圆
void Gui_Circle(u16 X, u16 Y, u16 R, u16 fc);

// 使用 Bresenham 算法绘制任意两点之间的直线
void Gui_DrawLine(u16 x0, u16 y0, u16 x1, u16 y1, u16 Color);

// 绘制一个带高光和阴影边框的矩形框
void Gui_box(u16 x, u16 y, u16 w, u16 h, u16 bc);

// 按指定模式绘制矩形边框
void Gui_box2(u16 x, u16 y, u16 w, u16 h, u8 mode);

// 绘制按下状态的按钮边框
void DisplayButtonDown(u16 x1, u16 y1, u16 x2, u16 y2);

// 绘制弹起状态的按钮边框
void DisplayButtonUp(u16 x1, u16 y1, u16 x2, u16 y2);

// 显示 16x16 GBK 汉字或 8x16 ASCII 字符串
void Gui_DrawFont_GBK16(u16 x, u16 y, u16 fc, u16 bc, u8 *s);

// 显示 24x24 GBK 汉字或 8x16 ASCII 字符串
void Gui_DrawFont_GBK24(u16 x, u16 y, u16 fc, u16 bc, u8 *s);

// 显示 32x32 数字点阵
void Gui_DrawFont_Num32(u16 x, u16 y, u16 fc, u16 bc, u16 num);

// 用指定颜色填充矩形区域
void LCD_Fill(u16 sx, u16 sy, u16 ex, u16 ey, u16 color);

// 使用当前前景色绘制直线
void LCD_DrawLine(u16 x1, u16 y1, u16 x2, u16 y2);

// 使用当前前景色绘制矩形边框
void LCD_DrawRectangle(u16 x1, u16 y1, u16 x2, u16 y2);

// 使用当前前景色填充矩形区域
void LCD_DrawFillRectangle(u16 x1, u16 y1, u16 x2, u16 y2);

// 绘制圆形，可通过 fill 参数选择空心或实心
void gui_circle(int xc, int yc, u16 c, int r, int fill);

// 使用当前前景色和背景色显示字符串
void LCD_ShowString(u16 x, u16 y, u8 size, u8 *p, u8 mode);

// 按指定前景色、背景色和字号显示字符串
void Show_Str(u16 x, u16 y, u16 fc, u16 bc, u8 *str, u8 size, u8 mode);

// 将字符串水平居中后显示
void Gui_StrCenter(u16 x, u16 y, u16 fc, u16 bc, u8 *str, u8 size, u8 mode);

// 显示 40x40 的 RGB565 图片
void Gui_Drawbmp16(u16 x, u16 y, const unsigned char *p);

#endif
