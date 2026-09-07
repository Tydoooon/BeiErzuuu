#ifndef UI_FONT_W25Q128_IMG_PATH
#error "UI_FONT_W25Q128_IMG_PATH must point to tools/ui_font_w25q128.img"
#endif

    .syntax unified
    .section .rodata.ui_font_w25q128_img, "a", %progbits
    .global g_ui_font_w25q128_img
    .global g_ui_font_w25q128_img_end
    .type g_ui_font_w25q128_img, %object
    .align 2
g_ui_font_w25q128_img:
    .incbin UI_FONT_W25Q128_IMG_PATH
g_ui_font_w25q128_img_end:
    .size g_ui_font_w25q128_img, g_ui_font_w25q128_img_end - g_ui_font_w25q128_img
