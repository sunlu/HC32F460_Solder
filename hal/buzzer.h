#ifndef HAL_BUZZER_H
#define HAL_BUZZER_H

#include "hc32_ll.h"

// 蜂鸣模式 无源(外部驱动) 谐振频率，1/2占空比。方波
// 鸣叫模式定义
typedef enum {
    BUZZ_OFF,
    BUZZ_ONCE,    // 单次鸣叫（使用 set_tone 设定的时间）
    BUZZ_DOUBLE,  // 双击确认音
    BUZZ_BOOT     // 开机三短鸣
} buzz_mode_t;

void buzzer_init(void);
void buzzer_set_tone(uint32_t frequency_hz, uint32_t duration_ms);
void buzzer_trigger(buzz_mode_t mode);

#endif
