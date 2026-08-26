#ifndef _LCD_DRIVER_H_
#define _LCD_DRIVER_H_

#include "main.h"
#include "cmsis_os.h"
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

#ifndef delay_ms
static inline void lcd_delay_ms(uint32_t ms)
{
    if (osKernelGetState() == osKernelRunning)
    {
        (void)osDelay(ms);
    }
    else
    {
        HAL_Delay(ms);
    }
}
#define delay_ms(ms) lcd_delay_ms(ms)
#endif

// Support portrait/landscape switching.
#define USE_HORIZONTAL 0

// Flip the panel top-to-bottom without changing the logical resolution.
#define LCD_FLIP_VERTICAL 1
// Flip the panel left-to-right without changing the logical resolution.
#define LCD_FLIP_HORIZONTAL 1

#define LCD_MADCTL_MY  0x80
#define LCD_MADCTL_MX  0x40
#define LCD_MADCTL_MV  0x20
#define LCD_MADCTL_BGR 0x08

#if USE_HORIZONTAL
#define LCD_MADCTL_BASE (LCD_MADCTL_MY | LCD_MADCTL_MX | LCD_MADCTL_MV | LCD_MADCTL_BGR)
#else
#define LCD_MADCTL_BASE (LCD_MADCTL_MX | LCD_MADCTL_BGR)
#endif

#if LCD_FLIP_VERTICAL
#define LCD_MADCTL_VERTICAL_FLIP LCD_MADCTL_MY
#else
#define LCD_MADCTL_VERTICAL_FLIP 0
#endif

#if LCD_FLIP_HORIZONTAL
#define LCD_MADCTL_HORIZONTAL_FLIP LCD_MADCTL_MX
#else
#define LCD_MADCTL_HORIZONTAL_FLIP 0
#endif

#define LCD_MADCTL_VALUE (LCD_MADCTL_BASE ^ LCD_MADCTL_VERTICAL_FLIP ^ LCD_MADCTL_HORIZONTAL_FLIP)

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

#define LCD_LED_PORT TFT_BL_GPIO_Port
#define LCD_LED TFT_BL_Pin
#define LCD_RS_PORT TFT_DC_GPIO_Port
#define LCD_RS TFT_DC_Pin
#define LCD_CS_PORT TFT_CS_GPIO_Port
#define LCD_CS TFT_CS_Pin
#define LCD_RST_PORT TFT_RES_GPIO_Port
#define LCD_RST TFT_RES_Pin

#define LCD_CS_SET HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS, GPIO_PIN_SET)
#define LCD_RS_SET HAL_GPIO_WritePin(LCD_RS_PORT, LCD_RS, GPIO_PIN_SET)
#define LCD_RST_SET HAL_GPIO_WritePin(LCD_RST_PORT, LCD_RST, GPIO_PIN_SET)
#define LCD_LED_SET HAL_GPIO_WritePin(LCD_LED_PORT, LCD_LED, GPIO_PIN_SET)

#define LCD_CS_CLR HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS, GPIO_PIN_RESET)
#define LCD_RS_CLR HAL_GPIO_WritePin(LCD_RS_PORT, LCD_RS, GPIO_PIN_RESET)
#define LCD_RST_CLR HAL_GPIO_WritePin(LCD_RST_PORT, LCD_RST, GPIO_PIN_RESET)
#define LCD_LED_CLR HAL_GPIO_WritePin(LCD_LED_PORT, LCD_LED, GPIO_PIN_RESET)

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
