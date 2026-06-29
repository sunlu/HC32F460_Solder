/**
 * @file    gui.c
 * @brief   AxxSolder-style GUI — direct rendering (no uGUI)
 */

#include "gui.h"
#include "config.h"
#include "heater.h"
#include "lcd.h"
#include "lcd_font.h"
#include "sensors.h"
#include "state.h"
#include <stdio.h>
#include <string.h>

#define SCR_W 320
#define SCR_H 240

#define TITLE_H 35
#define CTR_Y 38
#define INFO_Y 126
#define HINT_Y 180
#define DBG_Y 204

static SOLDER_STATE_Def pState = STATE_NONE;

void gui_show_startup_screen(void) {
  lcd_fill_screen(C_BLACK);
  lcd_draw_string_center(60, "JBC245", C_ORANGE, C_BLACK, &DefaultFont);
  lcd_draw_string_center(100, "Soldering Station", C_WHITE, C_BLACK, &DefaultFont);
  lcd_draw_string_center(125, "HC32F460", C_CYAN, C_BLACK, &DefaultFont);
  lcd_draw_string_center(155, "Initializing...", C_GRAY, C_BLACK, &DefaultFont);
}

void gui_draw_main_screen(void) {
  lcd_fill_screen(C_BLACK);
  lcd_fill_rect(0, 0, SCR_W, TITLE_H, C_TITLE_BG);
  lcd_draw_hline(0, TITLE_H, SCR_W, C_DIVIDER);
  lcd_draw_hline(0, CTR_Y - 2, SCR_W, C_DIVIDER);
  lcd_draw_hline(0, INFO_Y - 2, SCR_W, C_DIVIDER);
  lcd_draw_hline(0, HINT_Y - 2, SCR_W, C_DIVIDER);

  // lcd_draw_chinese(0, CTR_Y,  "你好中文", C_GRAY, C_BLACK, &FontChinese24,  &DefaultFont);

  lcd_draw_string(8, INFO_Y + 2, "Handle:", C_GRAY, C_BLACK, &DefaultFont);
  lcd_draw_string(8, INFO_Y + 20, "Power:", C_GRAY, C_BLACK, &DefaultFont);
  lcd_draw_string(180, INFO_Y + 2, "Vin:", C_GRAY, C_BLACK, &DefaultFont);
  lcd_draw_string(180, INFO_Y + 20, "State:", C_GRAY, C_BLACK, &DefaultFont);
  lcd_draw_string(10, HINT_Y + 4, "P1=250", C_GRAY, C_BLACK, &DefaultFont);
  lcd_draw_string(100, HINT_Y + 4, "P2=350", C_GRAY, C_BLACK, &DefaultFont);
  lcd_draw_string(190, HINT_Y + 4, "HOLD=MENU", C_GRAY, C_BLACK, &DefaultFont);
  // lcd_draw_rect(SCR_W - 55, 3, 48, TITLE_H - 6, C_FRAME);

  pState = STATE_NONE;
}

void gui_update_display(void) {

  char buf[48];

  SOLDER_STATE_Def state = sensor_val.current_state;

  uint8_t duty = heater_get_duty();

  /* Title */
  lcd_fill_rect(0, 0, 124, TITLE_H, C_TITLE_BG);
  snprintf(buf, sizeof(buf), "SET: %3.0f", sensor_val.temp_target);
  lcd_draw_string(4, 6, buf, C_YELLOW, C_TITLE_BG, &DefaultFont);

  lcd_draw_string_center(6, "Solder", C_WHITE, C_TITLE_BG, &DefaultFont);

  /* Power bar */

  int bx = SCR_W - 54, by = 4, bw = 46, bh = TITLE_H - 8;
  if (state == STATE_SLEEP || state == STATE_HALTED || state == STATE_EMERGENCY_SLEEP) {
    lcd_fill_rect(bx, by, bw, bh, C_ORANGE);
    lcd_draw_string(bx + 8, by + 6, (state == STATE_HALTED) ? "H" : "Z", C_BLACK, C_ORANGE, &DefaultFont);
  } else if (state == STATE_STANDBY || state == STATE_PRESTANDBY) {
    lcd_fill_rect(bx, by, bw, bh, C_ORANGE);
    lcd_draw_string(bx + 4, by + 8, "STBY", C_BLACK, C_ORANGE, &DefaultFont);
  } else {
    lcd_fill_rect(bx, by, bw, bh, C_BLACK);
    int fh = (int)(bh * duty / 100);
    if (fh > 0) {
      uint16_t bc = (duty > 80) ? C_RED : (duty > 50) ? C_ORANGE : (duty > 20) ? C_YELLOW : C_GREEN;
      lcd_fill_rect(bx, by + bh - fh, bw, fh, bc);
    }
  }
  snprintf(buf, sizeof(buf), (state == STATE_RUN) ? "%2d%%" : "OFF", duty);
  lcd_draw_string(bx - 40, by + 6, buf, C_WHITE, C_TITLE_BG, &DefaultFont);

  /* Center temp */

  lcd_fill_rect(0, CTR_Y, SCR_W, INFO_Y - 3 - CTR_Y, C_BLACK);
  if (state == STATE_SLEEP || state == STATE_HALTED || state == STATE_EMERGENCY_SLEEP) {
    lcd_draw_string_center(CTR_Y, "---", C_GRAY, C_BLACK, &FreeSansBold30pt7b);
  } else {
    float diff = sensor_val.temp_target - sensor_val.temp_show;
    uint16_t tc = (diff < 3 && diff > -3) ? C_GREEN : (diff > 0) ? C_CORNFLOWER : C_RED;
    snprintf(buf, sizeof(buf), "%3.0f", sensor_val.temp_show);
    lcd_draw_string_center(CTR_Y, buf, tc, C_BLACK, &FreeSansBold30pt7b);
  }

  /* Info */
  lcd_fill_rect(80, INFO_Y + 2, 50, 16, C_BLACK);
  if (sensor_val.handleType == HANDLE_NONE) {
    lcd_draw_string(80, INFO_Y + 2, "---", C_RED, C_BLACK, &DefaultFont);
  } else {
    lcd_draw_string(80, INFO_Y + 2, handle_name(sensor_val.handleType), C_GREEN, C_BLACK, &DefaultFont);
  }

  lcd_fill_rect(230, INFO_Y + 2, 50, 16, C_BLACK);
  snprintf(buf, sizeof(buf), "%.1fV", sensor_val.voltage);
  lcd_draw_string(230, INFO_Y + 2, buf, C_WHITE, C_BLACK, &DefaultFont);

  lcd_fill_rect(80, INFO_Y + 20, 90, 16, C_BLACK);
  snprintf(buf, sizeof(buf), "%2.0fW/%2d%%", sensor_val.power_avg, duty);
  lcd_draw_string(80, INFO_Y + 20, buf, C_WHITE, C_BLACK, &DefaultFont);

  if (state != pState) {
    const char *sn = state_name(state);
    uint16_t sc = C_GRAY;
    switch (state) {
    case STATE_RUN:
      sc = C_GREEN;
      break;
    case STATE_PRESTANDBY:
      sc = C_YELLOW;
      break;
    case STATE_STANDBY:
      sc = C_ORANGE;
      break;
    case STATE_SLEEP:
      sc = C_CYAN;
      break;
    case STATE_EMERGENCY_SLEEP:
      sc = C_RED;
      break;
    default:
      break;
    }
    lcd_fill_rect(230, INFO_Y + 20, 110, 16, C_BLACK);
    lcd_draw_string(230, INFO_Y + 20, sn, sc, C_BLACK, &DefaultFont);
  }

  /* Debug */
  lcd_fill_rect(0, DBG_Y, SCR_W, 30, C_BLACK);
  snprintf(buf, sizeof(buf), "P:%3.0f D:%2d%% I:%2.1f T:%3.0f/%3.0f SW:%0.1f / %0.1f",
    sensor_val.power_req, duty, sensor_val.current,
           sensor_val.temp_avg, PID_setpoint,
  sensor_val.sleep, sensor_val.replace
  );

  lcd_draw_string(4, DBG_Y, buf, C_GRAY, C_BLACK, &FreeSans6pt7b);
}

void gui_show_popup(const char *msg) {
  int px = (SCR_W - 260) / 2, py = (SCR_H - 50) / 2;
  lcd_fill_rect(px, py, 260, 50, C_GRAY);
  lcd_draw_rect(px - 1, py - 1, 262, 52, C_WHITE);
  lcd_draw_string(px + 40, py + 16, msg, C_WHITE, C_GRAY, &FreeSans6pt7b);
}