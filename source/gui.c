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

extern float pid_output;

#define SCR_W 320
#define SCR_H 240

#define TITLE_H 35
#define CTR_Y 38
#define INFO_Y 126
#define HINT_Y 180
#define DBG_Y 204

static uint8_t s_first = 1;
static float s_p_set = -1, s_p_act = -1, s_p_pct = -1, s_p_v = -1;
static m_state_t s_p_st = STATE_NONE;

void gui_show_startup_screen(void) {
  lcd_fill_screen(C_BLACK);
  lcd_draw_string_center(60, "JBC245", C_ORANGE, C_BLACK, &DefaultFont);
  // draw_str(60, 100, "Soldering Station", C_WHITE, C_BLACK);
  // draw_str(80, 125, "HC32F460", C_CYAN, C_BLACK);
  // draw_str(60, 155, "Initializing...", C_GRAY, C_BLACK);
}
void gui_init(void) { gui_show_startup_screen(); }

void gui_draw_main_screen(void) {
  lcd_fill_screen(C_BLACK);
  lcd_fill_rect(0, 0, SCR_W, TITLE_H, C_TITLE_BG);
  lcd_draw_hline(0, TITLE_H, SCR_W, C_DIVIDER);
  lcd_draw_hline(0, CTR_Y - 2, SCR_W, C_DIVIDER);
  lcd_draw_hline(0, INFO_Y - 2, SCR_W, C_DIVIDER);
  lcd_draw_hline(0, HINT_Y - 2, SCR_W, C_DIVIDER);

  lcd_draw_chinese(0, CTR_Y,  "你好中文", C_GRAY, C_BLACK, &FontChinese24,  &DefaultFont);

  lcd_draw_string(8, INFO_Y + 2, "Handle:", C_GRAY, C_BLACK, &DefaultFont);
  lcd_draw_string(8, INFO_Y + 18, "Power:", C_GRAY, C_BLACK, &DefaultFont);
  lcd_draw_string(160, INFO_Y + 2, "Vin:", C_GRAY, C_BLACK, &DefaultFont);
  lcd_draw_string(160, INFO_Y + 18, "State:", C_GRAY, C_BLACK, &DefaultFont);
  lcd_draw_string(10, HINT_Y + 4, "PRE1=250", C_GRAY, C_BLACK, &RobotoMono_Thin7pt7b);
  lcd_draw_string(120, HINT_Y + 4, "PRE2=350", C_GRAY, C_BLACK, &RobotoMono_Thin7pt7b);
  lcd_draw_string(230, HINT_Y + 4, "HOLD=MENU", C_GRAY, C_BLACK, &RobotoMono_Thin7pt7b);
  //lcd_draw_rect(SCR_W - 55, 3, 48, TITLE_H - 6, C_FRAME);
  s_first = 0;
  s_p_set = s_p_act = s_p_pct = s_p_v = -1;
  s_p_st = STATE_NONE;
}

void gui_update_display(void) {
  return;

  char buf[32];
  float act, set, v, w;
  m_state_t st;
  if (s_first)
    gui_draw_main_screen();
  act = sensor_val.temp_avg;
  set = sensor_val.temp_target;
  v = sensor_val.voltage;
  st = sensor_val.current_state;
  w = sensor_val.current * v;
  if (w < 0)
    w = 0;

  float pct = heater_get_duty();

  /* Title */
  if (set != s_p_set || st != s_p_st) {
    lcd_fill_rect(0, 0, 124, TITLE_H, C_TITLE_BG);
    if (st == STATE_SLEEP || st == STATE_HALTED ||
        st == STATE_EMERGENCY_SLEEP) {

      // draw_str(4, 6, "SET:---", C_GRAY, C_TITLE_BG);
    } else {
      snprintf(buf, sizeof(buf), "SET:%3.0f", set);
      // draw_str(4, 6, buf, C_YELLOW, C_TITLE_BG);
    }
    // draw_str(130, 6, "Solder", C_WHITE, C_TITLE_BG);
  }

  /* Power bar */
  if (pct != s_p_pct || st != s_p_st) {
    int bx = SCR_W - 54, by = 4, bw = 46, bh = TITLE_H - 8;
    if (st == STATE_SLEEP || st == STATE_HALTED ||
        st == STATE_EMERGENCY_SLEEP) {
      lcd_fill_rect(bx, by, bw, bh, C_ORANGE);
      // draw_str(bx + 8, by + 6, (st == STATE_HALTED) ? "H" : "Z", C_BLACK,
      // C_ORANGE);
    } else if (st == STATE_STANDBY || st == STATE_PRESTANDBY) {
      lcd_fill_rect(bx, by, bw, bh, C_ORANGE);
      // draw_str(bx + 4, by + 8, "STBY", C_BLACK, C_ORANGE);
    } else {
      lcd_fill_rect(bx, by, bw, bh, C_BLACK);
      int fh = (int)(bh * pct / 100.0f);
      if (fh > 0) {
        uint16_t bc = (pct > 80)   ? C_RED
                      : (pct > 50) ? C_ORANGE
                      : (pct > 20) ? C_YELLOW
                                   : C_GREEN;
        lcd_fill_rect(bx, by + bh - fh, bw, fh, bc);
      }
    }
    snprintf(
        buf, sizeof(buf),
        (st == STATE_SLEEP || st == STATE_HALTED || st == STATE_EMERGENCY_SLEEP)
            ? "OFF"
            : "%2.0f%%",
        pct);
    // draw_str(bx - 35, by + 6, buf, C_WHITE, C_TITLE_BG);
  }

  /* Center temp */
  if (act != s_p_act || st != s_p_st) {
    // lcd_fill_rect(0, CTR_Y, SCR_W, INFO_Y - 3 - CTR_Y, C_BLACK);
    if (st == STATE_SLEEP || st == STATE_HALTED ||
        st == STATE_EMERGENCY_SLEEP) {
      // draw_str(100, CTR_Y + 20, "---", C_GRAY, C_BLACK);
    } else {
      float err = set - act;
      uint16_t tc = (err < 3 && err > -3) ? C_GREEN
                    : (err > 0)           ? C_CORNFLOWER
                                          : C_RED;
      snprintf(buf, sizeof(buf), "%3.0f", act);
      // draw_str(60, CTR_Y + 20, buf, tc, C_BLACK);
      // draw_str(SCR_W - 25, CTR_Y + 20, "C", tc, C_BLACK);
    }
  }

  /* Info */
  lcd_fill_rect(50, INFO_Y + 2, 50, 16, C_BLACK);
  if (sensor_val.handleType == HANDLE_NONE) {
    // draw_str(60, INFO_Y + 2,   "---",  C_RED, C_BLACK);
  } else {
    // draw_str(60, INFO_Y + 2, handle_name(sensor_val.handleType), C_GREEN,
    // C_BLACK);
  }
  if (v != s_p_v || st != s_p_st) {
    // lcd_fill_rect(190,INFO_Y+2,50,16,C_BLACK);
    snprintf(buf, sizeof(buf), "%.1fV", v);
    // draw_str(210, INFO_Y + 2, buf, C_WHITE, C_BLACK);
  }
  if (pct != s_p_pct || st != s_p_st) {
    // lcd_fill_rect(60,INFO_Y+18,90,16,C_BLACK);
    snprintf(buf, sizeof(buf), "%2.1fW/%2.0f%%", w, pct);
    // draw_str(60, INFO_Y + 18, buf, C_WHITE, C_BLACK);
  }
  if (st != s_p_st) {
    const char *sn = state_name(st);
    uint16_t sc = C_GRAY;
    switch (st) {
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
    // lcd_fill_rect(200,INFO_Y+18,110,16,C_BLACK);
    // draw_str(210, INFO_Y + 18, (char *)sn, sc, C_BLACK);
  }

  /* Debug */
  // lcd_fill_rect(0,DBG_Y,SCR_W,16,C_BLACK);
  snprintf(buf, sizeof(buf), "PID:%3.0f DUT:%3.0f%% V:%.1f",
           sensor_val.power_req, pct, v);
  // draw_str(4, DBG_Y + 2, buf, C_GRAY, C_BLACK);

  s_p_set = set;
  s_p_act = act;
  s_p_pct = pct;
  s_p_v = v;
  s_p_st = st;
}

void gui_show_popup(const char *msg) {
  int px = (SCR_W - 260) / 2, py = (SCR_H - 50) / 2;
  lcd_fill_rect(px, py, 260, 50, C_GRAY);
  lcd_draw_rect(px - 1, py - 1, 262, 52, C_WHITE);
  // draw_str(px + 40, py + 16, (char *)msg, C_WHITE, C_GRAY);
}
void gui_draw_settings_menu(void) {
  lcd_fill_screen(C_BLACK);
  lcd_fill_rect(0, 0, SCR_W, 24, C_TITLE_BG);
  // draw_str(100, 2, "SETTINGS", C_WHITE, C_TITLE_BG);
  lcd_draw_hline(0, 24, SCR_W, C_DIVIDER);
}
void gui_update_settings_menu(uint8_t s, uint8_t l) {
  (void)s;
  (void)l;
}
