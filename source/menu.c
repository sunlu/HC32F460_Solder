/**
 * @file    menu.c
 * @brief   Soldering station settings menu (ported from AxxSolder)
 *
 * 3-level menu: Group / Parameter / Edit
 * Safety: heater is disabled on menu entry.
 */

#include "menu.h"
#include "config.h"
#include "encoder.h"
#include "gui.h"
#include "heater.h"
#include "lcd.h"
#include "lcd_font.h"
#include "state.h"
#include <stdio.h>
#include <string.h>

typedef enum {
  GRP_MODE = 0,
  GRP_PRESETS,
  GRP_DISPLAY,
  GRP_SOUND,
  GRP_SYSTEM,
  GRP_COUNT
} MenuGroup;

static const char *s_group_names[] = {"行为设置", "预设温度", "显示设置",
                                      "声音设置", "系统设置"};

#define MODE_ITEMS 11
#define PRESET_ITEMS 2
#define DISPLAY_ITEMS 4
#define SOUND_ITEMS 4
#define SYSTEM_ITEMS 3

static const char *s_mode_names[] = {
    "开机温度",      "温度偏移",    "待机温度", "待机时间(min)",
    "急停时间(min)", "开机蜂鸣",    "到达蜂鸣", "编码器反向",
    "休眠时间(min)", "待机延时(s)", "Boost温度"};
static const char *s_preset_names[] = {"预设1", "预设2"};
static const char *s_display_names[] = {"屏幕旋转", "温度单位", "功率单位",
                                        "显示曲线"};
static const char *s_sound_names[] = {"蜂鸣使能", "开机蜂鸣", "到达蜂鸣",
                                      "蜂鸣音调"};

typedef struct {
  float *ptr;
  float min, max, step;
  uint8_t is_int;
} ParamDef;

static const ParamDef s_mode_params[MODE_ITEMS] = {
    {&config_val.startup_temperature, 100, 480, 5, 1},
    {&config_val.temperature_offset, -50, 50, 1, 1},
    {&config_val.standby_temp, 50, 300, 5, 1},
    {&config_val.standby_time, 1, 60, 1, 1},
    {&config_val.emergency_time, 1, 120, 1, 1},
    {&config_val.startup_beep, 0, 1, 1, 1},
    {&config_val.beep_at_set_temp, 0, 1, 1, 1},
    {&config_val.change_enc_dir, 0, 1, 1, 1},
    {&config_val.sleep_timeout_min, 1, 120, 1, 1},
    {&config_val.standby_delay_s, 0, 30, 1, 1},
    {&config_val.boost_temp, 200, 480, 5, 1},
};
static const ParamDef s_preset_params[PRESET_ITEMS] = {
    {&config_val.temp_cal_100, 100, 480, 5, 1},
    {&config_val.temp_cal_200, 100, 480, 5, 1},
};
static const ParamDef s_display_params[DISPLAY_ITEMS] = {
    {&config_val.screen_rotation, 0, 3, 1, 1},
    {&config_val.temperature_offset, 0, 1, 1, 1},
    {&config_val.power_unit, 0, 1, 1, 1},
    {&config_val.display_graph, 0, 1, 1, 1},
};
static const ParamDef s_sound_params[SOUND_ITEMS] = {
    {&config_val.buzzer_enabled, 0, 1, 1, 1},
    {&config_val.startup_beep, 0, 1, 1, 1},
    {&config_val.beep_at_set_temp, 0, 1, 1, 1},
    {&config_val.beep_tone, 0, 3, 1, 1},
};

static const char *get_name(MenuGroup g, uint8_t i) {
  switch (g) {
  case GRP_MODE:
    return (i < MODE_ITEMS) ? s_mode_names[i] : "?";
  case GRP_PRESETS:
    return (i < PRESET_ITEMS) ? s_preset_names[i] : "?";
  case GRP_DISPLAY:
    return (i < DISPLAY_ITEMS) ? s_display_names[i] : "?";
  case GRP_SOUND:
    return (i < SOUND_ITEMS) ? s_sound_names[i] : "?";
  default:
    return "系统操作";
  }
}
static const ParamDef *get_def(MenuGroup g, uint8_t i) {
  switch (g) {
  case GRP_MODE:
    return (i < MODE_ITEMS) ? &s_mode_params[i] : NULL;
  case GRP_PRESETS:
    return (i < PRESET_ITEMS) ? &s_preset_params[i] : NULL;
  case GRP_DISPLAY:
    return (i < DISPLAY_ITEMS) ? &s_display_params[i] : NULL;
  case GRP_SOUND:
    return (i < SOUND_ITEMS) ? &s_sound_params[i] : NULL;
  default:
    return NULL;
  }
}
static uint8_t get_cnt(MenuGroup g) {
  switch (g) {
  case GRP_MODE:
    return MODE_ITEMS;
  case GRP_PRESETS:
    return PRESET_ITEMS;
  case GRP_DISPLAY:
    return DISPLAY_ITEMS;
  case GRP_SOUND:
    return SOUND_ITEMS;
  case GRP_SYSTEM:
    return SYSTEM_ITEMS;
  default:
    return 0;
  }
}

void menu_enter(void) {
  MenuGroup grp = GRP_MODE;
  uint8_t level = 0, cursor = 0, editing = 0;
  float edit_val = 0.0f;
  const ParamDef *pdef = NULL;
  char buf[48];

  float saved_setpoint = PID_setpoint;
  PID_setpoint = 0.0f;
  heater_off();

  lcd_fill_screen(C_BLACK);
  lcd_fill_rect(0, 0, TFT_WIDTH, 24, C_TITLE_BG);
  lcd_draw_chinese(4,0,"设置菜单",C_WHITE,C_TITLE_BG,&FontChinese24,&DefaultFont);
  lcd_draw_hline(0, 24, TFT_WIDTH, C_DIVIDER);
  encoder_get_count();

  for (;;) {
    int8_t rot = encoder_get_count();
    if (rot != 0) {
      if (editing) {
        const ParamDef *pd = get_def(grp, cursor);
        if (pd) {
          edit_val += rot * pd->step;
          if (edit_val < pd->min)
            edit_val = pd->min;
          if (edit_val > pd->max)
            edit_val = pd->max;
        }
      } else {
        uint8_t max = (level == 0) ? GRP_COUNT : get_cnt(grp) + 1;
        int16_t c = (int16_t)cursor + rot;
        if (c < 0)
          c = 0;
        if (c >= (int16_t)max)
          c = max - 1;
        cursor = (uint8_t)c;
      }
    }
    encoder_event_t evt = encoder_event();
    if (evt == ENC_SW_SHORT_PRESS) {
      if (editing) {
        const ParamDef *pd = get_def(grp, cursor);
        if (pd && pd->ptr)
          *pd->ptr = edit_val;
        editing = 0;
      } else if (level == 0) {
        grp = (MenuGroup)cursor;
        cursor = 0;
        level = 1;
      } else {
        uint8_t max = get_cnt(grp);
        if (cursor == max) {
          cursor = (uint8_t)grp;
          level = 0;
        } else if (grp == GRP_SYSTEM) {
          if (cursor == 0)
            NVIC_SystemReset();
          else if (cursor == 1)
            break;
          else {
            memcpy(&config_val, &config_val_default, sizeof(config_val));
            break;
          }
        } else {
          const ParamDef *pd = get_def(grp, cursor);
          if (pd && pd->ptr) {
            edit_val = *pd->ptr;
            pdef = pd;
            editing = 1;
          }
        }
      }
    }
    if (evt == ENC_SW_LONG_PRESS) {
      if (editing)
        editing = 0;
      else if (level > 0) {
        cursor = (uint8_t)grp;
        level = 0;
      } else
        break;
    }
    /* Render */
    lcd_fill_rect(0, 25, TFT_WIDTH, TFT_HEIGHT - 25, C_BLACK);
    uint8_t y = 30, lh = 22;
    if (level == 0) {
      for (uint8_t i = 0; i < GRP_COUNT; i++) {
        uint16_t bg = (i == cursor) ? C_TITLE_BG : C_BLACK,
                 fg = (i == cursor) ? C_WHITE : C_GRAY;
        lcd_fill_rect(4, y - 2, TFT_WIDTH - 8, lh, bg);
        lcd_draw_chinese(12,y,(char*)s_group_names[i],fg,bg,&FontChinese24,&DefaultFont); y+=lh+4;
      }
    } else if (editing) {
      const char *n = get_name(grp, cursor);
      snprintf(buf, sizeof(buf), "%s", n);
      lcd_draw_chinese(0,y,buf,C_WHITE,C_BLACK,&FontChinese24,&DefaultFont); y+=40;
      snprintf(buf, sizeof(buf), pdef && pdef->is_int ? "%3.0f" : "%.1f",
               edit_val);
      lcd_draw_chinese(0,y,buf,C_BLACK,C_WHITE,&FontChinese24,&DefaultFont); y+=55;
      lcd_draw_chinese(0,y,"短按确认 长按取消",C_GRAY,C_BLACK,&FontChinese24,&DefaultFont);
    } else {
      uint8_t max = get_cnt(grp);
      for (uint8_t i = 0; i < max; i++) {
        uint16_t bg = (i == cursor) ? C_TITLE_BG : C_BLACK,
                 fg = (i == cursor) ? C_WHITE : C_GRAY;
        lcd_fill_rect(4, y - 2, TFT_WIDTH - 8, lh, bg);
        const char *n = get_name(grp, i);
        const ParamDef *pd = get_def(grp, i);
        if (pd && pd->ptr) {
          snprintf(buf, sizeof(buf), pd->is_int ? "%s:%3.0f" : "%s:%.1f", n,
                   *pd->ptr);
        } else {
          snprintf(buf, sizeof(buf), "%s", n);
        }
        lcd_draw_chinese(12,y,buf,fg,bg,&FontChinese24,&DefaultFont); y+=lh+4;
      }
      uint16_t bg = (cursor == max) ? C_TITLE_BG : C_BLACK,
               fg = (cursor == max) ? C_WHITE : C_GRAY;
      lcd_fill_rect(4, y - 2, TFT_WIDTH - 8, lh, bg);
      lcd_draw_chinese(12,y,"返回",fg,bg,&FontChinese24,&DefaultFont);
    }
    //lcd_fill_rect(0, TFT_HEIGHT - 20, TFT_WIDTH, 20, C_BLACK);
    lcd_draw_chinese(TFT_HEIGHT-20,0,"旋转=选择 短按=确认长按=返回",C_GRAY,C_BLACK,&FontChinese24,&DefaultFont);
    DDL_DelayMS(30);
  }
  gui_draw_main_screen();
  PID_setpoint = saved_setpoint;
}
