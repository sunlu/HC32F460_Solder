/**
 * @file    sensors.h
 * @brief   传感器处理接口
 *
 * 将 ADC 原始值转换为物理量。
 */

#ifndef SOURCE_SENSORS_H
#define SOURCE_SENSORS_H

#include <stdio.h>
#include <string.h>
#include "hc32_ll.h"
#include "adc.h"
#include "mcu.h"
#include "average.h" 
#include "config.h"
#include "hysteresis.h"

extern t_adc_val s_adcVal;

void sensors_init(void);

void sensors_read_low(void);

void sensors_read_hight(void); 

const char *handle_name(HANDLE_TYPE_Def handle);

#endif /* SOURCE_SENSORS_H */
