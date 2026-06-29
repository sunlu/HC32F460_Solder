/**
 * @file    heater.h
 * @brief   HC32F460 加热器 PWM 控制接口
 *
 * NPN 驱动 (MMBT2222A) 反相：PA10 高电平 → NPN 导通 → PMOS 栅极低电平 → 加热开启。
 * TIMA-1-PWM3 输出于 PA10 (GPIO_FUNC_3)：高电平时间 = 加热。
 */

#ifndef SOURCE_HEATER_H
#define SOURCE_HEATER_H

#include "hc32_ll.h"

void heater_init(void);
void heater_set_duty(uint16_t duty);  /* 0=关闭, HEATER_PWM_PERIOD=全开 */
void heater_off(void);
uint8_t heater_get_duty(void);

#endif /* SOURCE_HEATER_H */

