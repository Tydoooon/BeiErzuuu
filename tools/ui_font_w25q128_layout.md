# UI Font W25Q128 Layout

Generate `ui_font_16.bin` on the PC, then write:

- Header: `0x00050000`
- Font data: `0x00051000`

Header format is `app_w25q128_font_header_t`:

```c
typedef struct
{
    uint32_t magic;       // APP_W25Q128_FONT_MAGIC, 0x46545731
    uint16_t version;     // APP_W25Q128_FONT_VERSION, 1
    uint16_t header_size; // sizeof(app_w25q128_font_header_t)
    uint32_t data_addr;   // APP_W25Q128_FONT16_DATA_ADDR, 0x00051000
    uint32_t data_size;   // ui_font_16.bin size
    uint32_t data_crc32;  // IEEE CRC32 of font data
    char name[32];        // "ui_font_16.bin"
} app_w25q128_font_header_t;
```

Command example:

```powershell
.\tools\make_ui_font.ps1 -Font C:\Windows\Fonts\simhei.ttf
```

Do not compile `ui_font_16.bin` into MCU internal Flash. Write it to W25Q128 with a serial/download tool or a temporary updater command.
