/**
 * @file    gui.h
 * @brief   GUI interface — direct rendering
 */
#ifndef SOURCE_GUI_H
#define SOURCE_GUI_H

#include "main.h"

/* RGB565 colors */
#define C_BLACK       0x0000
#define C_WHITE       0xFFFF
#define C_RED         0xF800
#define C_GREEN       0x07E0
#define C_BLUE        0x001F
#define C_YELLOW      0xFFE0
#define C_CYAN        0x07FF
#define C_ORANGE      0xFD20
#define C_GRAY        0x8410
#define C_DARK_GRAY   0x4208
#define C_TITLE_BG    0x39C7
#define C_DIVIDER     0x6B4D
#define C_FRAME       0x8410
#define C_CORNFLOWER  0x6499

void gui_init(void);
void gui_draw_main_screen(void);
void gui_update_display(void);
void gui_show_popup(const char *msg);
void gui_show_startup_screen(void);
void gui_draw_settings_menu(void);
void gui_update_settings_menu(uint8_t selection, uint8_t level);

#endif
