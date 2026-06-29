/**
 * @file    adc.h
 * @brief   HC32F460 的 ADC 驱动接口
 *
 * 使用 ADC1 以软件触发单次采样 
 */

#ifndef SOURCE_ADC_H
#define SOURCE_ADC_H

#include "hc32_ll.h"

#define ADC_VREF                3.3f
#define ADC_RESOLUTION          4096



typedef struct {
  uint16_t V; /*电压采样*/
  uint16_t A; /*电流采样*/
  uint16_t C; /*温度采样*/
} t_adc_val;

/* 函数声明 */
void adc_init(void);
void adc_read_channel(uint8_t u8Ch, uint16_t *pu16Value);
void adc_read_all(t_adc_val* data);
uint16_t adc_read_single(uint8_t u8Ch);

#endif /* SOURCE_ADC_H */
