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
 
#define PWM_PERIOD 999 

static uint16_t s_u16Compare = 0;
static bool s_bTimerStarted = false;

void heater_on(void);
void heater_off(void);

void heater_init(void) {
  stc_gpio_init_t stcGpioInit;
  stc_tmra_init_t stcTmraInit;
  stc_tmra_pwm_init_t stcPwmInit;

  /* 1. Enable TimerA1 clock */
  FCG_Fcg2PeriphClockCmd(FCG2_PERIPH_TMRA_1, ENABLE);

  /* 2. Configure PA10 as GPIO output LOW (safety: ensure MOSFET off at startup)
   */
  GPIO_StructInit(&stcGpioInit);
  stcGpioInit.u16PinDir = PIN_DIR_OUT;
  stcGpioInit.u16PinOutputType = PIN_OUT_TYPE_CMOS;
  stcGpioInit.u16PinDrv =
      PIN_HIGH_DRV; /* HIGH drive for clean MOSFET switching */
  GPIO_Init(HEATER_PWM_PORT, HEATER_PWM_PIN, &stcGpioInit);
  GPIO_ResetPins(HEATER_PWM_PORT, HEATER_PWM_PIN);

  /* 3. Configure TimerA base */
  (void)TMRA_StructInit(&stcTmraInit);
  stcTmraInit.u8CountSrc = TMRA_CNT_SRC_SW;
  stcTmraInit.sw_count.u8CountMode = TMRA_MD_SAWTOOTH;
  stcTmraInit.sw_count.u8CountDir = TMRA_DIR_UP;
  stcTmraInit.sw_count.u8ClockDiv = TMRA_CLK_DIV64;
  stcTmraInit.u32PeriodValue = PWM_PERIOD;
  (void)TMRA_Init(HEATER_TIMER_UNIT, &stcTmraInit);

  /* 4. Configure PWM output */
  (void)TMRA_PWM_StructInit(&stcPwmInit);
  stcPwmInit.u32CompareValue = 0U;
  stcPwmInit.u16StartPolarity = TMRA_PWM_HIGH;
  stcPwmInit.u16StopPolarity = TMRA_PWM_LOW;
  stcPwmInit.u16CompareMatchPolarity = TMRA_PWM_LOW;
  stcPwmInit.u16PeriodMatchPolarity = TMRA_PWM_HIGH;
  (void)TMRA_PWM_Init(HEATER_TIMER_UNIT, HEATER_TIMER_CH, &stcPwmInit);

  /* 5. Switch pin to timer function */
  GPIO_SetFunc(HEATER_PWM_PORT, HEATER_PWM_PIN, GPIO_FUNC_4);

  /* 6. Ensure heater OFF on startup */
  heater_off();
}

void heater_on(void) {
  if (!s_bTimerStarted) {
    //TMRA_SetCountValue(HEATER_TIMER_UNIT, 0U);
    TMRA_PWM_OutputCmd(HEATER_TIMER_UNIT, HEATER_TIMER_CH, ENABLE);
    TMRA_Start(HEATER_TIMER_UNIT);
    s_bTimerStarted = true;
  }
}

void heater_off(void) {
  TMRA_PWM_OutputCmd(HEATER_TIMER_UNIT, HEATER_TIMER_CH, DISABLE);
  TMRA_Stop(HEATER_TIMER_UNIT);
  TMRA_SetCountValue(HEATER_TIMER_UNIT, 0U);
  s_bTimerStarted = false;
  s_u16Compare = 0;
}

void heater_set_duty(uint16_t duty) {
  if (duty <= 1) {
    heater_off();
    return;
  }
  if (duty > 30) {
    duty = 30;
  }
  
  uint16_t compareValue = (uint16_t)(duty * 10);

  if (s_u16Compare == compareValue)
    return;

  s_u16Compare = compareValue;

  TMRA_SetCompareValue(HEATER_TIMER_UNIT, HEATER_TIMER_CH, s_u16Compare);
  heater_on();
}

uint8_t heater_get_duty(void) { return (uint8_t)(s_u16Compare * 100 /(PWM_PERIOD+1)); }
