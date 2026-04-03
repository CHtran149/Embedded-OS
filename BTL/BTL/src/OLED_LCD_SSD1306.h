#ifndef __OLED_LCD_SSD1306_H
#define __OLED_LCD_SSD1306_H 

#include <stdint.h> // Thay cho các kiểu dữ liệu của HAL
#include <stdlib.h>
#include <string.h>
#include "fonts.h"

/* SSD1306 width in pixels */
#define SSD1306_WIDTH            128  // Nên để 128 cho chuẩn SSD1306
#define SSD1306_HEIGHT           64

#define SSD1306_I2C_ADDR 0x3C 
#define OLED_DEVICE_PATH "/dev/oled_ssd1306"

typedef enum { SSD1306_COLOR_BLACK = 0, SSD1306_COLOR_WHITE = 1 } SSD1306_COLOR_t;

typedef struct {
    uint16_t CurrentX;
    uint16_t CurrentY;
    uint8_t Inverted;
    uint8_t Initialized;
    uint8_t SSD1306_Buffer[SSD1306_WIDTH * SSD1306_HEIGHT / 8];
} SSD1306_Name;

// Các hàm giữ nguyên interface để bạn không phải sửa code logic
void SSD1306_UpdateScreen(SSD1306_Name* SSD1306);
uint8_t SSD1306_Init(SSD1306_Name* SSD1306); // Bỏ tham số I2C_HandleTypeDef
void SSD1306_Fill(SSD1306_Name* SSD1306, SSD1306_COLOR_t Color);
void SSD1306_DrawPixel(SSD1306_Name* SSD1306, uint16_t x, uint16_t y, SSD1306_COLOR_t color);
void SSD1306_GotoXY(SSD1306_Name* SSD1306, uint16_t x, uint16_t y);
char SSD1306_Putc(SSD1306_Name* SSD1306, char ch, FontDef_t* Font, SSD1306_COLOR_t color);  
char SSD1306_Puts(SSD1306_Name* SSD1306, char* str, FontDef_t* Font, SSD1306_COLOR_t color);
void SSD1306_Clear(SSD1306_Name* SSD1306);

#endif
