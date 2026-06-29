/**
 * @file    encoder.h
 * @brief   EC11旋转编码器
 */

#ifndef SOURCE_ENCODER_H
#define SOURCE_ENCODER_H

#include "hc32_ll.h"

  
// 按键事件类型
typedef enum {
    ENC_SW_NONE = 0,
    ENC_SW_SHORT_PRESS,    // 短按
    ENC_SW_LONG_PRESS,     // 长按
    ENC_SW_RELEASE,        // 释放（可选）
} encoder_event_t;
 

void encoder_init(void); 
int8_t encoder_get_count(void); 
encoder_event_t  encoder_event(void);

#endif /* SOURCE_ENCODER_H */
