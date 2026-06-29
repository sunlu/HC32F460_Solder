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

typedef enum { GRP_MODE = 0, GRP_PRESETS, GRP_DISPLAY, GRP_SOUND, GRP_SYSTEM, GRP_COUNT } MenuGroup;

static const char *s_group_names[] = {"行为设置", "预设温度", "显示设置", "声音设置", "系统设置"};

#define MODE_ITEMS 11
#define PRESET_ITEMS 2
#define DISPLAY_ITEMS 4
#define SOUND_ITEMS 4
#define SYSTEM_ITEMS 2

static const char *s_mode_names[] = {"开机温度",      "温度偏移",    "待机温度", "待机时间(min)",
                                     "急停时间(min)", "开机蜂鸣",    "到达蜂鸣", "编码器反向",
                                     "休眠时间(min)", "待机延时(s)", "Boost温度"};
static const char *s_preset_names[] = {"预设1", "预设2"};
static const char *s_display_names[] = {"屏幕旋转", "温度单位", "功率单位", "显示曲线"};
static const char *s_sound_names[] = {"蜂鸣使能", "开机蜂鸣", "到达蜂鸣", "蜂鸣音调"};

static const char *s_system_names[] = {"Reset", "Reboot" };

typedef enum {
  TINT,
  TFLOAT,
  TBOOL
} ParamType;

typedef struct {
  float *ptr;
  float min, max, step;
  ParamType type;
} ParamDef;

static const ParamDef s_mode_params[MODE_ITEMS] = {
    {&config_val.startup_temperature, 100, 480, 5, TINT}, 
    {&config_val.temperature_offset, -50, 50, 1, TINT},
    {&config_val.standby_temp, 50, 300, 5, TINT},         
    {&config_val.standby_time, 1, 60, 1, TINT},
    {&config_val.emergency_time, 1, 120, 1, TINT},        
    {&config_val.startup_beep, 0, 1, 1, TBOOL},
    {&config_val.beep_at_set_temp, 0, 1, 1, TINT},        
    {&config_val.change_enc_dir, 0, 1, 1, TBOOL},
    {&config_val.sleep_timeout_min, 1, 120, 1, TINT},     
    {&config_val.standby_delay_s, 0, 30, 1, TINT},
    {&config_val.boost_temp, 200, 480, 5, TINT},
};
static const ParamDef s_preset_params[PRESET_ITEMS] = {
    {&config_val.temp_cal_100, 100, 480, 5, TINT},
    {&config_val.temp_cal_200, 100, 480, 5, TINT},
};
static const ParamDef s_display_params[DISPLAY_ITEMS] = {
    {&config_val.screen_rotation, 0, 3, 1, 1},
    {&config_val.temperature_offset, 0, 1, 1, 1},
    {&config_val.power_unit, 0, 1, 1, 1},
    {&config_val.display_graph, 0, 1, 1, 1},
};
static const ParamDef s_sound_params[SOUND_ITEMS] = {
    {&config_val.buzzer_enabled, 0, 1, 1, TBOOL},
    {&config_val.startup_beep, 0, 1, 1, TBOOL},
    {&config_val.beep_at_set_temp, 0, 1, 1, TBOOL},
    {&config_val.beep_tone, 0, 3, 1, TINT},
};

static const ParamDef s_system_params[SYSTEM_ITEMS] = {
    {0, 0, 1, 1, TBOOL},
    {0, 0, 1, 1, TBOOL}
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
   case GRP_SYSTEM:
    return (i < SYSTEM_ITEMS) ? s_system_names[i] : "?";
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
  case GRP_SYSTEM:
    return (i < SYSTEM_ITEMS) ? &s_system_params[i] : NULL;
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

static MenuGroup grp = GRP_MODE;
static uint8_t level = 0, cursor = 0, editing = 0;
static float edit_val = 0.0f;
static const ParamDef *pdef = NULL;

static int formatParam(char* buf, size_t size,const ParamDef *p, float val){
    if(!p)
     return snprintf(buf, size,  "%.1f", val);
    
    
    switch(p->type){
      case TBOOL:
        return  snprintf(buf, size,  "%s", val > 0.5 ? "True":"False");
       case TINT:
        return  snprintf(buf, size,  "%3.0f", val);
    }
    
    return snprintf(buf, size,  "%.1f", val);
}

static void render() {
  /* Render */
  char buf[48];

  lcd_fill_rect(0, 30, TFT_WIDTH, TFT_HEIGHT - 30, C_BLACK);
  uint16_t y = 35, lh = 28;
  uint8_t max_visible = 6; // 屏幕最多显示 6 行

  if (level == 0) {
    for (uint8_t i = 0; i < GRP_COUNT; i++) {
      uint16_t bg = (i == cursor) ? C_TITLE_BG : C_BLACK, fg = (i == cursor) ? C_WHITE : C_GRAY;
      lcd_fill_rect(4, y - 2, TFT_WIDTH - 8, lh, bg);
      lcd_draw_chinese(12, y, (char *)s_group_names[i], fg, bg, &FontChinese24, &DefaultFont);
      y += lh;
    }
  } else if (editing) {

    lcd_draw_chinese(0, y, get_name(grp, cursor), C_WHITE, C_BLACK, &FontChinese24, &DefaultFont);
    y += lh;
    
    formatParam(buf,sizeof(buf),pdef,edit_val);
    
    //snprintf(buf, sizeof(buf), pdef && pdef->is_int ? "%3.0f" : "%.1f", edit_val);
    
    lcd_draw_string_center(60, buf, C_GREEN, C_BLACK, &FreeSansBold30pt7b); 
  } else {
    uint8_t lines = get_cnt(grp);

    uint8_t index = 0;

    if (cursor > 2 && lines > max_visible) {
      index = cursor - 2;
      if (index > lines - max_visible)
        index = lines - max_visible;
    }

    uint16_t bg, fg;
    const char *n;
    const ParamDef *pd;

    for (uint8_t i = 0; i < max_visible; i++) {

      uint16_t bg = (index + i == cursor) ? C_TITLE_BG : C_BLACK, fg = (index + i == cursor) ? C_WHITE : C_GRAY;
      lcd_fill_rect(4, y - 2, TFT_WIDTH - 8, lh, bg);
      n = get_name(grp, index + i);
      pd = get_def(grp, index + i);
      
      lcd_draw_chinese(12, y, n, fg, bg, &FontChinese24, &DefaultFont);
      
      //formatParam(buf,sizeof(buf),pd, pd->ptr ? *pd->ptr : 0); 
      
      //lcd_draw_string_right(  y, buf, fg, bg,  &DefaultFont);
      y += lh;

      if (y > TFT_HEIGHT)
        break;
    }
  }
  lcd_fill_rect(0, TFT_HEIGHT - 24, TFT_WIDTH, 24, C_BLACK);
  lcd_draw_chinese(0, TFT_HEIGHT - 24, "旋转=选择 短按=确认 长按=返回", C_GRAY, C_BLACK, &FontChinese24, &DefaultFont);
}

void menu_enter(void) {

  float saved_setpoint = PID_setpoint;
  PID_setpoint = 0.0f;
  heater_off();

  lcd_fill_screen(C_BLACK);
  lcd_fill_rect(0, 0, TFT_WIDTH, 24, C_TITLE_BG);
  lcd_draw_chinese(4, 0, "设置菜单", C_WHITE, C_TITLE_BG, &FontChinese24, &DefaultFont);
  lcd_draw_hline(0, 24, TFT_WIDTH, C_DIVIDER);
  int8_t rot = encoder_get_count();
  encoder_event_t evt = encoder_event();
  render();

  for (;;) {
    rot = encoder_get_count();
    evt = encoder_event();
    
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
    }else if (evt == ENC_SW_SHORT_PRESS) {
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
    }else if (evt == ENC_SW_LONG_PRESS) {
      if (editing)
        editing = 0;
      else if (level > 0) {
        cursor = (uint8_t)grp;
        level = 0;
      } else
        break;
    }

    if (evt != ENC_SW_NONE || rot != 0) {
      render();
    }

    DDL_DelayMS(30);
  }
  gui_draw_main_screen();
  PID_setpoint = saved_setpoint;
}
