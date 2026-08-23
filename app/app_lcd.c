#include "app_lcd.h"
#include "bsp_lcd.h"

u16 ColorTab[5] = {BRED, YELLOW, RED, GREEN, BLUE};

void DrawTestPage(u8 *str)
{
    LCD_Fill(0, 0, lcddev.width - 1, 20, BLUE);
    LCD_Fill(0, lcddev.height - 20, lcddev.width - 1, lcddev.height - 1, BLUE);
    LCD_Fill(0, 21, lcddev.width - 1, lcddev.height - 21, BLACK);

    POINT_COLOR = WHITE;
    Gui_StrCenter(0, 2, WHITE, BLUE, str, 16, 1);
    Gui_StrCenter(0, lcddev.height - 18, WHITE, BLUE, (u8 *)"QDtech", 16, 1);
}

void main_test(void)
{
    DrawTestPage((u8 *)"ILI9341 Test");
    Gui_StrCenter(0, 50, RED, BLACK, (u8 *)"LCD Demo", 16, 1);
    Gui_StrCenter(0, 80, YELLOW, BLACK, (u8 *)"ILI9341 240X320", 16, 1);
    Gui_StrCenter(0, 110, GREEN, BLACK, (u8 *)"STM32F407 HAL", 16, 1);
    delay_ms(800);
}

void Test_Color(void)
{
    DrawTestPage((u8 *)"Color Test");

    LCD_Fill(0, 21, lcddev.width - 1, lcddev.height - 21, WHITE);
    Show_Str(lcddev.width - 60, 30, BLUE, YELLOW, (u8 *)"White", 16, 1);
    delay_ms(500);

    LCD_Fill(0, 21, lcddev.width - 1, lcddev.height - 21, BLACK);
    Show_Str(lcddev.width - 60, 30, BLUE, YELLOW, (u8 *)"Black", 16, 1);
    delay_ms(500);

    LCD_Fill(0, 21, lcddev.width - 1, lcddev.height - 21, RED);
    Show_Str(lcddev.width - 60, 30, BLUE, YELLOW, (u8 *)"Red", 16, 1);
    delay_ms(500);

    LCD_Fill(0, 21, lcddev.width - 1, lcddev.height - 21, GREEN);
    Show_Str(lcddev.width - 60, 30, BLUE, YELLOW, (u8 *)"Green", 16, 1);
    delay_ms(500);

    LCD_Fill(0, 21, lcddev.width - 1, lcddev.height - 21, BLUE);
    Show_Str(lcddev.width - 60, 30, WHITE, YELLOW, (u8 *)"Blue", 16, 1);
    delay_ms(500);
}

void Test_FillRec(void)
{
    u8 i;
    uint16_t cx = lcddev.width / 2;
    uint16_t cy = lcddev.height / 2;

    DrawTestPage((u8 *)"Rectangle Test");
    LCD_Fill(0, 21, lcddev.width - 1, lcddev.height - 21, WHITE);

    for (i = 0; i < 5; i++)
    {
        POINT_COLOR = ColorTab[i];
        LCD_DrawRectangle(cx - 80 + i * 15, cy - 80 + i * 15,
                          cx - 20 + i * 15, cy - 20 + i * 15);
    }
    delay_ms(800);

    LCD_Fill(0, 21, lcddev.width - 1, lcddev.height - 21, WHITE);
    for (i = 0; i < 5; i++)
    {
        POINT_COLOR = ColorTab[i];
        LCD_DrawFillRectangle(cx - 80 + i * 15, cy - 80 + i * 15,
                              cx - 20 + i * 15, cy - 20 + i * 15);
    }
    delay_ms(800);
}

void Test_Circle(void)
{
    u8 i;
    uint16_t cx = lcddev.width / 2;
    uint16_t cy = lcddev.height / 2;

    DrawTestPage((u8 *)"Circle Test");
    LCD_Fill(0, 21, lcddev.width - 1, lcddev.height - 21, WHITE);

    for (i = 0; i < 5; i++)
    {
        gui_circle(cx - 80 + i * 35, cy - 40 + i * 15, ColorTab[i], 25, 0);
    }
    delay_ms(800);

    LCD_Fill(0, 21, lcddev.width - 1, lcddev.height - 21, WHITE);
    for (i = 0; i < 5; i++)
    {
        gui_circle(cx - 80 + i * 35, cy - 40 + i * 15, ColorTab[i], 25, 1);
    }
    delay_ms(800);
}

void English_Font_test(void)
{
    DrawTestPage((u8 *)"English Test");
    POINT_COLOR = RED;
    BACK_COLOR = BLUE;
    LCD_ShowString(10, 40, 12, (u8 *)"6X12:abcdefghijklmnopqrstuvwxyz", 0);
    LCD_ShowString(10, 60, 12, (u8 *)"6X12:ABCDEFGHIJKLMNOPQRSTUVWXYZ", 1);
    LCD_ShowString(10, 85, 16, (u8 *)"8X16:0123456789", 0);
    LCD_ShowString(10, 110, 16, (u8 *)"8X16:~!@#$%^&*()", 1);
    delay_ms(800);
}

void Chinese_Font_test(void)
{
    DrawTestPage((u8 *)"Font Test");
    Show_Str(10, 40, BLUE, YELLOW, (u8 *)"16X16:Welcome", 16, 1);
    Show_Str(10, 70, BLUE, YELLOW, (u8 *)"24X24:ILI9341", 24, 1);
    Show_Str(10, 110, BLUE, YELLOW, (u8 *)"32X32:LCD", 32, 1);
    delay_ms(800);
}

void Pic_test(void)
{
    uint16_t x1 = 30;
    uint16_t x2 = (lcddev.width > 130) ? 90 : 30;
    uint16_t x3 = (lcddev.width > 190) ? 150 : 30;

    DrawTestPage((u8 *)"Picture Test");
    LCD_Fill(0, 21, lcddev.width - 1, lcddev.height - 21, WHITE);
    Gui_Drawbmp16(x1, 40, gImage_qq);
    Gui_Drawbmp16(x2, 100, gImage_qq);
    Gui_Drawbmp16(x3, 160, gImage_qq);
    delay_ms(800);
}
