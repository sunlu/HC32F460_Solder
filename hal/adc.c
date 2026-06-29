/**
 * @file    adc.c
 * @brief   ADC driver implementation for HC32F460 (DDL Rev3.3.0)
 * 
 */

#include "adc.h"
 
#define VIN_ADC_PORT            GPIO_PORT_B
#define VIN_ADC_PIN             GPIO_PIN_00

#define TEMP_ADC_PORT           GPIO_PORT_A
#define TEMP_ADC_PIN            GPIO_PIN_07

#define CURRENT_ADC_PORT        GPIO_PORT_A
#define CURRENT_ADC_PIN         GPIO_PIN_06

//==============================================================================
// ADC
//==============================================================================
#define ADC_CH_TEMP             ADC_CH7
#define ADC_CH_CURRENT          ADC_CH6
#define ADC_CH_VIN              ADC_CH8 

#define ADC_UNIT                CM_ADC1

 
void adc_init(void)
{
    stc_adc_init_t stcAdcInit;
    stc_gpio_init_t stcGpioInit;

    /* --- 1. 启用 ADC1 外设时钟（使用默认 PCLK 时钟源） --- */
    FCG_Fcg3PeriphClockCmd(FCG3_PERIPH_ADC1, ENABLE);

    /* --- 2. 配置引脚为模拟输入模式 --- */
    GPIO_StructInit(&stcGpioInit);
    stcGpioInit.u16PinAttr = PIN_ATTR_ANALOG;
    stcGpioInit.u16PinDir  = PIN_DIR_IN;

    /* PA6 (ADC12_IN6) 电流, PA7 (ADC12_IN7) 温度 */
    GPIO_Init(TEMP_ADC_PORT, TEMP_ADC_PIN | CURRENT_ADC_PIN, &stcGpioInit);

    /* PB0 (ADC12_IN8) 电压 */
    GPIO_Init(VIN_ADC_PORT, VIN_ADC_PIN, &stcGpioInit);

    /* --- 3. 初始化 ADC 通用配置 --- */
    (void)ADC_StructInit(&stcAdcInit);
    stcAdcInit.u16ScanMode = ADC_MD_SEQA_SINGLESHOT;
    stcAdcInit.u16Resolution = ADC_RESOLUTION_12BIT;
    stcAdcInit.u16DataAlign = ADC_DATAALIGN_RIGHT;
    (void)ADC_Init(ADC_UNIT, &stcAdcInit);

    /* --- 4. 启用通道 --- */
    ADC_ChCmd(ADC_UNIT, ADC_SEQ_A, ADC_CH_TEMP, ENABLE);
    ADC_ChCmd(ADC_UNIT, ADC_SEQ_A, ADC_CH_VIN, ENABLE);
    ADC_ChCmd(ADC_UNIT, ADC_SEQ_A, ADC_CH_CURRENT, ENABLE);

    /* --- 5. 配置采样时间（热电偶为高阻抗信号，需要较长采样时间） --- */
    ADC_SetSampleTime(ADC_UNIT, ADC_CH_TEMP, 0x80U);
    ADC_SetSampleTime(ADC_UNIT, ADC_CH_VIN, 0x40U);
    ADC_SetSampleTime(ADC_UNIT, ADC_CH_CURRENT, 0x40U);

    /* --- 6. 启用硬件平均（32 次平均，提高信噪比） --- */
    ADC_ConvDataAverageConfig(ADC_UNIT, ADC_AVG_CNT32);
    ADC_ConvDataAverageChCmd(ADC_UNIT, ADC_CH_TEMP, ENABLE);
    ADC_ConvDataAverageChCmd(ADC_UNIT, ADC_CH_VIN, ENABLE);
    ADC_ConvDataAverageChCmd(ADC_UNIT, ADC_CH_CURRENT, ENABLE);

    /* --- 7. 关闭中断（使用轮询模式） --- */
    ADC_AWD_IntCmd(ADC_UNIT, ADC_AWD_INT_SEQA, DISABLE);
    ADC_IntCmd(ADC_UNIT, ADC_INT_EOCA, DISABLE);
}

 
void adc_read_all(t_adc_val* data)
{
    uint32_t u32Timeout = 100000UL;

    if (data == NULL) return;
 
    ADC_Start(ADC_UNIT);
 
    while (ADC_GetStatus(ADC_UNIT, ADC_FLAG_EOCA) == RESET)
    {
        if (--u32Timeout == 0UL) 
        {
            return;  
        }
    }
 
    ADC_ClearStatus(ADC_UNIT, ADC_FLAG_EOCA);
     
    data->V = ADC_GetValue(ADC_UNIT, ADC_CH_VIN);
    data->A = ADC_GetValue(ADC_UNIT, ADC_CH_CURRENT);
    data->C = ADC_GetValue(ADC_UNIT, ADC_CH_TEMP); 
}


uint16_t adc_read_single(uint8_t u8Ch)
{
    uint32_t u32Timeout = 100000UL;

    ADC_Start(ADC_UNIT);

    while (ADC_GetStatus(ADC_UNIT, ADC_FLAG_EOCA) == RESET)
    {
        if (--u32Timeout == 0UL) return 0U;
    }

    ADC_ClearStatus(ADC_UNIT, ADC_FLAG_EOCA);

    return ADC_GetValue(ADC_UNIT, u8Ch);
}

 
void adc_read_channel(uint8_t u8Ch, uint16_t *pu16Value)
{
    if (pu16Value != NULL)
    {
        *pu16Value = adc_read_single(u8Ch);
    }
}


