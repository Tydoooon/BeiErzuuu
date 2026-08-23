#include "bsp_lcd.h"
#include "spi.h"

_lcd_dev lcddev = {X_MAX_PIXEL, Y_MAX_PIXEL, 0x2C, 0x2A, 0x2B, USE_HORIZONTAL};
u16 POINT_COLOR = BLACK;
u16 BACK_COLOR = WHITE;

static void LCD_WriteByte(u8 data);

static void LCD_WriteByte(u8 data)
{
    if (HAL_SPI_Transmit(&hspi1, &data, 1, 100) != HAL_OK)
    {
        Error_Handler();
    }
}

void Lcd_WriteIndex(u8 Index)
{
    LCD_CS_CLR;
    LCD_RS_CLR;
    LCD_WriteByte(Index);
    LCD_CS_SET;
}

void Lcd_WriteData(u8 Data)
{
    LCD_CS_CLR;
    LCD_RS_SET;
    LCD_WriteByte(Data);
    LCD_CS_SET;
}

void Lcd_WriteData_16Bit(u16 Data)
{
    LCD_CS_CLR;
    LCD_RS_SET;
    LCD_WriteByte((u8)(Data >> 8));
    LCD_WriteByte((u8)Data);
    LCD_CS_SET;
}

void Lcd_WriteReg(u8 Index, u16 Data)
{
    Lcd_WriteIndex(Index);
    Lcd_WriteData_16Bit(Data);
}

void Lcd_Reset(void)
{
    LCD_RST_CLR;
    delay_ms(100);
    LCD_RST_SET;
    delay_ms(50);
}

void Lcd_Init(void)
{
    LCD_CS_SET;
    LCD_LED_SET;
    Lcd_Reset();

    Lcd_WriteIndex(0x11);
    Lcd_WriteData(0x00);
    delay_ms(120);

    Lcd_WriteIndex(0xCF);
    Lcd_WriteData(0X00);
    Lcd_WriteData(0XC1);
    Lcd_WriteData(0X30);

    Lcd_WriteIndex(0xED);
    Lcd_WriteData(0X64);
    Lcd_WriteData(0X03);
    Lcd_WriteData(0X12);
    Lcd_WriteData(0X81);

    Lcd_WriteIndex(0xE8);
    Lcd_WriteData(0X85);
    Lcd_WriteData(0X11);
    Lcd_WriteData(0X78);

    Lcd_WriteIndex(0xF6);
    Lcd_WriteData(0X01);
    Lcd_WriteData(0X30);
    Lcd_WriteData(0X00);

    Lcd_WriteIndex(0xCB);
    Lcd_WriteData(0X39);
    Lcd_WriteData(0X2C);
    Lcd_WriteData(0X00);
    Lcd_WriteData(0X34);
    Lcd_WriteData(0X05);

    Lcd_WriteIndex(0xF7);
    Lcd_WriteData(0X20);

    Lcd_WriteIndex(0xEA);
    Lcd_WriteData(0X00);
    Lcd_WriteData(0X00);

    Lcd_WriteIndex(0xC0);
    Lcd_WriteData(0X20);

    Lcd_WriteIndex(0xC1);
    Lcd_WriteData(0X11);

    Lcd_WriteIndex(0xC5);
    Lcd_WriteData(0X31);
    Lcd_WriteData(0X3C);

    Lcd_WriteIndex(0xC7);
    Lcd_WriteData(0XA9);

    Lcd_WriteIndex(0x3A);
    Lcd_WriteData(0X55);

    Lcd_WriteIndex(0x36);
#if USE_HORIZONTAL
    Lcd_WriteData(0xE8);
#else
    Lcd_WriteData(0x48);
#endif

    Lcd_WriteIndex(0xB1);
    Lcd_WriteData(0X00);
    Lcd_WriteData(0X18);

    Lcd_WriteIndex(0xB4);
    Lcd_WriteData(0X00);
    Lcd_WriteData(0X00);

    Lcd_WriteIndex(0xF2);
    Lcd_WriteData(0X00);

    Lcd_WriteIndex(0x26);
    Lcd_WriteData(0X01);

    Lcd_WriteIndex(0xE0);
    Lcd_WriteData(0X0F);
    Lcd_WriteData(0X17);
    Lcd_WriteData(0X14);
    Lcd_WriteData(0X09);
    Lcd_WriteData(0X0C);
    Lcd_WriteData(0X06);
    Lcd_WriteData(0X43);
    Lcd_WriteData(0X75);
    Lcd_WriteData(0X36);
    Lcd_WriteData(0X08);
    Lcd_WriteData(0X13);
    Lcd_WriteData(0X05);
    Lcd_WriteData(0X10);
    Lcd_WriteData(0X0B);
    Lcd_WriteData(0X08);

    Lcd_WriteIndex(0xE1);
    Lcd_WriteData(0X00);
    Lcd_WriteData(0X1F);
    Lcd_WriteData(0X23);
    Lcd_WriteData(0X03);
    Lcd_WriteData(0X0E);
    Lcd_WriteData(0X04);
    Lcd_WriteData(0X39);
    Lcd_WriteData(0X25);
    Lcd_WriteData(0X4D);
    Lcd_WriteData(0X06);
    Lcd_WriteData(0X0D);
    Lcd_WriteData(0X0B);
    Lcd_WriteData(0X33);
    Lcd_WriteData(0X37);
    Lcd_WriteData(0X0F);

    Lcd_WriteIndex(0x29);
    delay_ms(20);
}

void Lcd_SetXY(u16 Xpos, u16 Ypos)
{
    Lcd_SetRegion(Xpos, Ypos, Xpos, Ypos);
}

void Lcd_SetRegion(u16 xStar, u16 yStar, u16 xEnd, u16 yEnd)
{
    Lcd_WriteIndex(lcddev.setxcmd);
    Lcd_WriteData_16Bit(xStar);
    Lcd_WriteData_16Bit(xEnd);

    Lcd_WriteIndex(lcddev.setycmd);
    Lcd_WriteData_16Bit(yStar);
    Lcd_WriteData_16Bit(yEnd);

    Lcd_WriteIndex(lcddev.wramcmd);
}

void Gui_DrawPoint(u16 x, u16 y, u16 Data)
{
    Lcd_SetXY(x, y);
    Lcd_WriteData_16Bit(Data);
}

void LCD_DrawPoint(u16 x, u16 y)
{
    Gui_DrawPoint(x, y, POINT_COLOR);
}

void Lcd_Clear(u16 Color)
{
    u32 i;

    Lcd_SetRegion(0, 0, lcddev.width - 1, lcddev.height - 1);
    LCD_CS_CLR;
    LCD_RS_SET;

    for (i = 0; i < (u32)lcddev.width * lcddev.height; i++)
    {
        LCD_WriteByte((u8)(Color >> 8));
        LCD_WriteByte((u8)Color);
    }

    LCD_CS_SET;
}
