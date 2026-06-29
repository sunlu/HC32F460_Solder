#ifndef HAL_LCD_H
#define HAL_LCD_H

#include "hc32_ll.h"
#include "lcd_font.h"
//==============================================================================
// 显示屏 (ST7789)
//==============================================================================
#define TFT_WIDTH               320
#define TFT_HEIGHT              240
#define TFT_ROTATE              2       /* 0=竖屏0x00, 1=0xE0, 2=0x20, 3=0x00 */

/* Color definitions (RGB565) */
#define COLOR_BLACK       0x0000
#define COLOR_WHITE       0xFFFF
#define COLOR_RED         0xF800
#define COLOR_GREEN       0x07E0
#define COLOR_BLUE        0x001F
#define COLOR_YELLOW      0xFFE0
#define COLOR_CYAN        0x07FF
#define COLOR_MAGENTA     0xF81F
#define COLOR_ORANGE      0xFD20
#define COLOR_GRAY        0x8410
#define COLOR_DARK_GRAY   0x4208
#define COLOR_BG          0x18E3

//https://github.com/hhui112/LCD_hc32f460/blob/main/projects/ev_hc32f460_lqfp100_v2/examples/usart/usart_uart_int/source/lcd/GC9a01_drv.h

void lcd_init(void);
void lcd_set_window(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
void lcd_write_pixel(uint16_t x, uint16_t y, uint16_t color);
void lcd_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void lcd_draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void lcd_fill_screen(uint16_t color);
 
void lcd_draw_hline(uint16_t x, uint16_t y, uint16_t w, uint16_t color);
void lcd_draw_vline(uint16_t x, uint16_t y, uint16_t h, uint16_t color);
void lcd_set_brightness(uint8_t u8Pct);

//void lcd_fill_dma(uint16_t color);

uint8_t lcd_draw_char(uint16_t x, uint16_t y, const char c, uint16_t color, uint16_t bgColor, const GFXfont *font);
void lcd_draw_ch_char(uint16_t x, uint16_t y, const uint8_t *utf8_code, uint16_t color, uint16_t bgColor, const CHSFont *font);


void lcd_draw_chinese(uint16_t x, uint16_t y, const unsigned char *text, uint16_t color, uint16_t bgColor,const CHSFont *chFont,const GFXfont *font);
void lcd_draw_string(uint16_t x, uint16_t y, const unsigned char *text, uint16_t color,uint16_t bgColor,const GFXfont *font );

void lcd_draw_string_center(uint16_t y, const unsigned char *text,  uint16_t color, uint16_t bgColor,const GFXfont *font);
void lcd_draw_chinese_center(uint16_t y, const unsigned char *text, uint16_t color,uint16_t bgColor,const CHSFont *chFont,const  GFXfont *font); 
#endif
