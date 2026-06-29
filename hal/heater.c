/**
 * @file    heater.c
 * @brief   HC32F460 Heater PWM driver (DDL Rev3.3.0)
 * PA10 -> CM_TMRA_1_PWM3
 * NPN(MMBT2222A) inverts: PA10 HIGH -> NPN ON -> PMOS gate LOW -> heater ON
 */

#include "heater.h"
#include "stdbool.h"

#define HEATER_PWM_PORT GPIO_PORT_A
#define HEATER_PWM_PIN GPIO_PIN_10

#define HEATER_TIMER_UNIT CM_TMRA_1
#define HEATER_TIMER_CH TMRA_CH3

#define HEATER_PERIOD 6500U

static uint16_t s_u16CurrentDuty = 0;
static bool s_bTimerStarted = false;

void heater_on(void);
void heater_off(void);

void heater_init(void) {
  stc_gpio_init_t stcGpioInit;
  stc_tmra_init_t stcTmraInit;
  stc_tmra_pwm_init_t stcPwmInit;

  // 1. 使能 TimerA1 时钟
  FCG_Fcg2PeriphClockCmd(FCG2_PERIPH_TMRA_1, ENABLE);

  // 2. 初始配置 PA10 为普通 GPIO 输出低电平（确保刚上电绝对安全）
  GPIO_StructInit(&stcGpioInit);
  stcGpioInit.u16PinDir = PIN_DIR_OUT;
  stcGpioInit.u16PinOutputType = PIN_OUT_TYPE_CMOS;
  GPIO_Init(HEATER_PWM_PORT, HEATER_PWM_PIN, &stcGpioInit);
  GPIO_ResetPins(HEATER_PWM_PORT, HEATER_PWM_PIN); // 取消注释，显式清零

  // 3. 配置 TimerA 基础参数
  (void)TMRA_StructInit(&stcTmraInit);
  stcTmraInit.u8CountSrc = TMRA_CNT_SRC_SW;
  stcTmraInit.sw_count.u8CountMode = TMRA_MD_SAWTOOTH;
  stcTmraInit.sw_count.u8CountDir = TMRA_DIR_UP;
  stcTmraInit.sw_count.u8ClockDiv = TMRA_CLK_DIV256;
  stcTmraInit.u32PeriodValue = HEATER_PERIOD;
  (void)TMRA_Init(HEATER_TIMER_UNIT, &stcTmraInit);

  // 4. 先配置 PWM 输出极性，保持引脚完全被定时器接管
  (void)TMRA_PWM_StructInit(&stcPwmInit);
  stcPwmInit.u32CompareValue = 0U;                   // 初始占空比为0
  stcPwmInit.u16StartPolarity = TMRA_PWM_LOW;       // 计数开始输出高电平
  stcPwmInit.u16StopPolarity = TMRA_PWM_LOW;         // 停止时强制输出低电平
  stcPwmInit.u16CompareMatchPolarity = TMRA_PWM_HIGH; // 比较匹配时翻转为低电平
  stcPwmInit.u16PeriodMatchPolarity = TMRA_PWM_LOW; // 周期结束时翻转为高电平
  (void)TMRA_PWM_Init(HEATER_TIMER_UNIT, HEATER_TIMER_CH, &stcPwmInit);

  // 5. 开启 PWM 输出使能（让定时器牢牢锁死引脚电平为 StopPolarity 即低电平）
  TMRA_PWM_OutputCmd(HEATER_TIMER_UNIT, HEATER_TIMER_CH, ENABLE);

  // 6. 切换到定时器复用功能
  GPIO_SetFunc(HEATER_PWM_PORT, HEATER_PWM_PIN, GPIO_FUNC_4);

  /* 7. Ensure heater OFF on startup */
  heater_off();
}

void heater_on(void) {
  if (!s_bTimerStarted) {
    // 始终保持 TMRA_PWM_OutputCmd 为 ENABLE 状态，绝对不要 DISABLE 它
    TMRA_Start(HEATER_TIMER_UNIT);
    s_bTimerStarted = true;
  }
}

void heater_off(void) {
  // 安全关闭法：不关闭输出门，仅停止计数，硬件会自动强制拉低引脚（因为 u16StopPolarity = TMRA_PWM_LOW）
  TMRA_Stop(HEATER_TIMER_UNIT);
  TMRA_SetCountValue(HEATER_TIMER_UNIT, 0U); 
  // 同时将比较值清零，双重保险
  TMRA_SetCompareValue(HEATER_TIMER_UNIT, HEATER_TIMER_CH, 0U);
  
  s_bTimerStarted = false;
  s_u16CurrentDuty = 0;
}

void heater_set_duty(uint16_t duty) {
  if (duty <= 1) {
    heater_off();
    return;
  }
  
  // T12 软件限幅（数据手册建议或者根据供电限幅，防止大功率击穿）
  if (duty > 30) {
    duty = 30;
  }
 
  uint16_t compareValue = duty * 65;

  if (s_u16CurrentDuty == compareValue)
    return;

  s_u16CurrentDuty = compareValue;

  TMRA_SetCompareValue(HEATER_TIMER_UNIT, HEATER_TIMER_CH, s_u16CurrentDuty);
  heater_on();
}

uint8_t heater_get_duty(void) { 
  return (uint8_t)(s_u16CurrentDuty * 100 / (HEATER_PERIOD + 1)); 
}