#ifndef __GUI_H
#define __GUI_H

#include "bsp_lcd.h"
#include "pic.h"

u16 LCD_BGR2RGB(u16 c);
void Gui_Circle(u16 X,u16 Y,u16 R,u16 fc); 
void Gui_DrawLine(u16 x0, u16 y0,u16 x1, u16 y1,u16 Color);  
void Gui_box(u16 x, u16 y, u16 w, u16 h,u16 bc);
void Gui_box2(u16 x,u16 y,u16 w,u16 h, u8 mode);
void DisplayButtonDown(u16 x1,u16 y1,u16 x2,u16 y2);
void DisplayButtonUp(u16 x1,u16 y1,u16 x2,u16 y2);
void Gui_DrawFont_GBK16(u16 x, u16 y, u16 fc, u16 bc, u8 *s);
void Gui_DrawFont_GBK24(u16 x, u16 y, u16 fc, u16 bc, u8 *s);
void Gui_DrawFont_Num32(u16 x, u16 y, u16 fc, u16 bc, u16 num) ;

void LCD_Fill(u16 sx, u16 sy, u16 ex, u16 ey, u16 color);
void LCD_DrawLine(u16 x1, u16 y1, u16 x2, u16 y2);
void LCD_DrawRectangle(u16 x1, u16 y1, u16 x2, u16 y2);
void LCD_DrawFillRectangle(u16 x1, u16 y1, u16 x2, u16 y2);
void gui_circle(int xc, int yc, u16 c, int r, int fill);
void LCD_ShowString(u16 x, u16 y, u8 size, u8 *p, u8 mode);
void Show_Str(u16 x, u16 y, u16 fc, u16 bc, u8 *str, u8 size, u8 mode);
void Gui_StrCenter(u16 x, u16 y, u16 fc, u16 bc, u8 *str, u8 size, u8 mode);
void Gui_Drawbmp16(u16 x, u16 y, const unsigned char *p);

#endif
