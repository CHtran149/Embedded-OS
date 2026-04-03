#include "OLED_LCD_SSD1306.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

// Hàm ghi lệnh/dữ liệu trên Linux sẽ thông qua Driver
// Vì Driver của bạn đã tự Init khi insmod, nên App chỉ cần gửi dữ liệu
extern int fd_oled; 


void SSD1306_UpdateScreen(SSD1306_Name* SSD1306)
{
    if (fd_oled < 0) return;

    if (write(fd_oled, SSD1306->SSD1306_Buffer, sizeof(SSD1306->SSD1306_Buffer)) < 0) {
        perror("OLED: Write failed");
    }
}

uint8_t SSD1306_Init(SSD1306_Name* SSD1306) {
    // Trên Linux, Driver đã làm hết phần Init cứng
    // App chỉ cần reset buffer và biến trạng thái
    memset(SSD1306->SSD1306_Buffer, 0, sizeof(SSD1306->SSD1306_Buffer));
    SSD1306->CurrentX = 0;
    SSD1306->CurrentY = 0;
    SSD1306->Inverted = 0;
    SSD1306->Initialized = 1;
    return 0;
}

// --- Các hàm Fill, DrawPixel, Putc, Puts giữ nguyên logic của bạn ---
void SSD1306_Fill(SSD1306_Name* SSD1306, SSD1306_COLOR_t color) {
    memset(SSD1306->SSD1306_Buffer, (color == SSD1306_COLOR_BLACK) ? 0x00 : 0xFF, sizeof(SSD1306->SSD1306_Buffer));
}

void SSD1306_DrawPixel(SSD1306_Name* SSD1306, uint16_t x, uint16_t y, SSD1306_COLOR_t color) {
    if(x >= SSD1306_WIDTH || y >= SSD1306_HEIGHT) return;
    if (SSD1306->Inverted) color = (SSD1306_COLOR_t)!color;
    
    if (color == SSD1306_COLOR_WHITE) 
        SSD1306->SSD1306_Buffer[x + (y / 8) * SSD1306_WIDTH] |= 1 << (y % 8);
    else 
        SSD1306->SSD1306_Buffer[x + (y / 8) * SSD1306_WIDTH] &= ~(1 << (y % 8));
}

void SSD1306_GotoXY(SSD1306_Name* SSD1306, uint16_t x, uint16_t y) {
    SSD1306->CurrentX = x;
    SSD1306->CurrentY = y;
}

char SSD1306_Putc(SSD1306_Name* SSD1306, char ch, FontDef_t* Font, SSD1306_COLOR_t color) {
    uint32_t i, b, j;
    if(SSD1306_WIDTH <= (SSD1306->CurrentX + Font->FontWidth) || SSD1306_HEIGHT <= (SSD1306->CurrentY + Font->FontHeight)) 
        return 0;

    for (i = 0; i < Font->FontHeight; i++) {
        b = Font->data[(ch - 32) * Font->FontHeight + i];
        for (j = 0; j < Font->FontWidth; j++) {
            if ((b << j) & 0x8000) 
                SSD1306_DrawPixel(SSD1306, SSD1306->CurrentX + j, (SSD1306->CurrentY + i), color);
            // else 
            //     SSD1306_DrawPixel(SSD1306, SSD1306->CurrentX + j, (SSD1306->CurrentY + i), (SSD1306_COLOR_t)!color);
        }
    }
    SSD1306->CurrentX += Font->FontWidth;
    return ch;
}

char SSD1306_Puts(SSD1306_Name* SSD1306, char* str, FontDef_t* Font, SSD1306_COLOR_t color) {
    while (*str) {
        if (SSD1306_Putc(SSD1306, *str, Font, color) != *str) return *str;
        str++;
    }
    return *str;
}

void SSD1306_Clear(SSD1306_Name* SSD1306) {
    SSD1306_Fill(SSD1306, SSD1306_COLOR_BLACK);
    SSD1306_UpdateScreen(SSD1306);
}
