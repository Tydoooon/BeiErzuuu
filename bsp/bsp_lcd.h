#ifndef _LCD_DRIVER_H_
#define _LCD_DRIVER_H_

#include "main.h"
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

#ifndef delay_ms
#define delay_ms(ms) HAL_Delay(ms)
#endif

// Support portrait/landscape switching.
#define USE_HORIZONTAL 0

// Set to 1 to use SPI1 hardware output instead of bit-banged SPI.
#define USE_HARDWARE_SPI 1

#define LCD_X_SIZE 240
#define LCD_Y_SIZE 320

#if USE_HORIZONTAL
#define X_MAX_PIXEL LCD_Y_SIZE
#define Y_MAX_PIXEL LCD_X_SIZE
#else
#define X_MAX_PIXEL LCD_X_SIZE
#define Y_MAX_PIXEL LCD_Y_SIZE
#endif

#define RED 0xf800
#define GREEN 0x07e0
#define BLUE 0x001f
#define WHITE 0xffff
#define BLACK 0x0000
#define YELLOW 0xFFE0
#define GRAY0 0xEF7D
#define GRAY1 0x8410
#define GRAY2 0x4208

#define LCD_CTRL1 GPIOA
#define LCD_CTRL2 GPIOB
#define LCD_LED GPIO_PIN_0
#define LCD_RS GPIO_PIN_1
#define LCD_CS_PORT GPIOA
#define LCD_CS GPIO_PIN_3
#define LCD_RST GPIO_PIN_0
#define LCD_SCL GPIO_PIN_5
#define LCD_SDA GPIO_PIN_7

#define LCD_CS_SET HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS, GPIO_PIN_SET)
#define LCD_RS_SET HAL_GPIO_WritePin(LCD_CTRL2, LCD_RS, GPIO_PIN_SET)
#define LCD_SDA_SET HAL_GPIO_WritePin(LCD_CTRL1, LCD_SDA, GPIO_PIN_SET)
#define LCD_SCL_SET HAL_GPIO_WritePin(LCD_CTRL1, LCD_SCL, GPIO_PIN_SET)
#define LCD_RST_SET HAL_GPIO_WritePin(LCD_CTRL2, LCD_RST, GPIO_PIN_SET)
#define LCD_LED_SET HAL_GPIO_WritePin(LCD_CTRL1, LCD_LED, GPIO_PIN_SET)

#define LCD_CS_CLR HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS, GPIO_PIN_RESET)
#define LCD_RS_CLR HAL_GPIO_WritePin(LCD_CTRL2, LCD_RS, GPIO_PIN_RESET)
#define LCD_SDA_CLR HAL_GPIO_WritePin(LCD_CTRL1, LCD_SDA, GPIO_PIN_RESET)
#define LCD_SCL_CLR HAL_GPIO_WritePin(LCD_CTRL1, LCD_SCL, GPIO_PIN_RESET)
#define LCD_RST_CLR HAL_GPIO_WritePin(LCD_CTRL2, LCD_RST, GPIO_PIN_RESET)
#define LCD_LED_CLR HAL_GPIO_WritePin(LCD_CTRL1, LCD_LED, GPIO_PIN_RESET)

typedef struct
{
    u16 width;
    u16 height;
    u16 wramcmd;
    u16 setxcmd;
    u16 setycmd;
    u8 dir;
} _lcd_dev;

extern _lcd_dev lcddev;
extern u16 POINT_COLOR;
extern u16 BACK_COLOR;

void LCD_GPIO_Init(void);
void Lcd_WriteIndex(u8 Index);
void Lcd_WriteData(u8 Data);
void Lcd_WriteReg(u8 Index, u16 Data);
void Lcd_Reset(void);
void Lcd_Init(void);
void Lcd_Clear(u16 Color);
void Lcd_SetXY(u16 x, u16 y);
void Gui_DrawPoint(u16 x, u16 y, u16 Data);
void Lcd_SetRegion(u16 xStar, u16 yStar, u16 xEnd, u16 yEnd);
void Lcd_WriteData_16Bit(u16 Data);

void LCD_DrawPoint(u16 x, u16 y);

#define LCD_Init Lcd_Init
#define LCD_Clear Lcd_Clear
#define LCD_SetCursor Lcd_SetXY
#define LCD_SetWindows Lcd_SetRegion
#define LCD_WR_REG Lcd_WriteIndex
#define LCD_WR_DATA Lcd_WriteData
#define LCD_WR_DATA_16Bit Lcd_WriteData_16Bit

#endif
