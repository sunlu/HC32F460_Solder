/**
 * @file    adc.h
 * @brief   HC32F460 的 ADC 驱动接口
 *
 * 使用 ADC1 以软件触发单次采样 
 */

#ifndef SOURCE_MCU_H
#define SOURCE_MCU_H

#include "hc32_ll.h"

 
/* 函数声明 */
void mcu_init(void);
float mcu_get_temp(void); 

#endif /* SOURCE_ADC_H */
