/**
 * @file    encoder.c
 * @brief   EC11 旋转编码器
 *
 * 硬件：EC11 正交编码器（带按键）
 * A 相 → PA1 (TIMA-2-CLKB)
 * B 相 → PA0 (TIMA-2-CLKA)
 * 按键 → PA2 (GPIO_INPUT, 定时器读取状态)
 * 公共端 → GND
 */

#include "encoder.h"

// --- EC11旋转编码器 引脚定义 ---
#define ENC_SW_PORT GPIO_PORT_A
#define ENC_A_PIN GPIO_PIN_01
#define ENC_B_PIN GPIO_PIN_00
#define ENC_SW_PIN GPIO_PIN_02

/* ========== 定时器参数（10ms中断） ========== */
#define ENC_SW_TMR0_UNIT CM_TMR0_2
#define ENC_SW_TMR0_CH TMR0_CH_A
#define ENC_SW_TMR0_IRQn INT006_IRQn
#define ENC_SW_TMR0_INT_SRC INT_SRC_TMR0_2_CMP_A
#define ENC_SW_TMR0_INT_FLAG TMR0_FLAG_CMP_A
#define ENC_SW_TMR0_PCLK 10000000UL // FIXME: 请根据实际PCLK3频率修改
#define ENC_SW_TMR0_DIV TMR0_CLK_DIV1024
#define ENC_SW_TMR0_PERIOD ((ENC_SW_TMR0_PCLK / 256 / 100) - 1) // 10ms

/* ========== 状态机阈值（单位：10ms） ========== */
#define ENC_SW_DEBOUNCE_TIME 2 // 去抖 20ms
#define ENC_SW_SHORT_TIME 5    // 最短有效按下 50ms
#define ENC_SW_LONG_TIME 50    // 长按阈值 500ms

/* ========== 状态机变量 ========== */
typedef enum {
  ENC_SW_IDLE,
  ENC_SW_DEBOUNCE,
  ENC_SW_PRESSED,
  ENC_SW_LONG_PRESSED, // 已触发长按，等待释放
} encoder_sw_state_t;

static encoder_sw_state_t s_eState = ENC_SW_IDLE;
static uint16_t s_u16PressTime = 0;
static encoder_event_t s_eEvent = ENC_SW_NONE; // 当前待处理事件

static uint16_t s_u16LastCount = 0; // 记录上一次读取时的硬件计数值

// 如果发现顺逆时针反了，将这两个宏定义对调即可
#define ENC_TIMER_UNIT CM_TMRA_2
#define ENC_UP_COND (TMRA_CNT_DOWN_COND_CLKB_HIGH_CLKA_RISING)
#define ENC_DOWN_COND (TMRA_CNT_UP_COND_CLKA_HIGH_CLKB_RISING)

static void _encoder_sw_IRQCallback(void);
static void _encoder_sw_Scan(void);
static void _encoder_key_init(void);

static void _encoder_sw_Scan(void) {
  uint8_t pin_level = GPIO_ReadInputPins(ENC_SW_PORT, ENC_SW_PIN);

  switch (s_eState) {
  case ENC_SW_IDLE:
    if (pin_level == PIN_RESET) { // 按下（低电平有效）
      s_eState = ENC_SW_DEBOUNCE;
      s_u16PressTime = 0;
    }
    break;

  case ENC_SW_DEBOUNCE:
    s_u16PressTime++;
    if (s_u16PressTime >= ENC_SW_DEBOUNCE_TIME) {
      if (pin_level == PIN_RESET) {
        s_eState = ENC_SW_PRESSED;
        s_u16PressTime = 0;
      } else {
        s_eState = ENC_SW_IDLE; // 抖动，回到空闲
      }
    }
    break;

  case ENC_SW_PRESSED:
    s_u16PressTime++;
    if (pin_level == PIN_SET) { // 释放
      if (s_u16PressTime >= ENC_SW_SHORT_TIME) {
        s_eEvent = ENC_SW_SHORT_PRESS;
      }
      s_eState = ENC_SW_IDLE;
    } else if (s_u16PressTime >= ENC_SW_LONG_TIME) {
      s_eEvent = ENC_SW_LONG_PRESS;
      s_eState = ENC_SW_LONG_PRESSED; // 进入长按已触发状态
    }
    break;

  case ENC_SW_LONG_PRESSED:
    if (pin_level == PIN_SET) { // 释放
      s_eState = ENC_SW_IDLE;
    }
    break;

  default:
    break;
  }
}

static void _encoder_sw_IRQCallback(void) {
  TMR0_ClearStatus(ENC_SW_TMR0_UNIT, ENC_SW_TMR0_INT_FLAG);
  _encoder_sw_Scan();
}

static void _encoder_key_init(void) {
  stc_gpio_init_t stcGpioInit;
  stc_tmr0_init_t stcTmr0Init;
  stc_irq_signin_config_t stcIrq;

  /* 使能 Timer0 时钟 */
  FCG_Fcg2PeriphClockCmd(FCG2_PERIPH_TMR0_2, ENABLE);
  FCG_Fcg0PeriphClockCmd(FCG0_PERIPH_AOS, ENABLE);

  /* 配置 PA02 为输入，内部上拉 */
  GPIO_StructInit(&stcGpioInit);
  stcGpioInit.u16PinDir = PIN_DIR_IN;
  stcGpioInit.u16PullUp = PIN_PU_ON;
  GPIO_Init(ENC_SW_PORT, ENC_SW_PIN, &stcGpioInit);

  /* 配置 Timer0 */
  TMR0_StructInit(&stcTmr0Init);
  stcTmr0Init.u32ClockSrc = TMR0_CLK_SRC_INTERN_CLK;
  stcTmr0Init.u32ClockDiv = ENC_SW_TMR0_DIV;
  stcTmr0Init.u32Func = TMR0_FUNC_CMP;
  stcTmr0Init.u16CompareValue = (uint16_t)ENC_SW_TMR0_PERIOD;
  TMR0_Init(ENC_SW_TMR0_UNIT, ENC_SW_TMR0_CH, &stcTmr0Init);

  // AOS_SetTriggerEventSrc(AOS_TMR0, BSP_KEY_KEY10_EVT);

  /* 注册中断 */
  stcIrq.enIntSrc = ENC_SW_TMR0_INT_SRC;
  stcIrq.enIRQn = ENC_SW_TMR0_IRQn;
  stcIrq.pfnCallback = &_encoder_sw_IRQCallback;
  INTC_IrqSignIn(&stcIrq);

  NVIC_ClearPendingIRQ(stcIrq.enIRQn);
  NVIC_SetPriority(stcIrq.enIRQn, DDL_IRQ_PRIO_DEFAULT - 1);
  NVIC_EnableIRQ(stcIrq.enIRQn);

  TMR0_IntCmd(ENC_SW_TMR0_UNIT, TMR0_INT_CMP_A, ENABLE);
  TMR0_Start(ENC_SW_TMR0_UNIT, ENC_SW_TMR0_CH);
}

void encoder_init(void) {
  /* 【重要修改】 优先使能 GPIO 时钟 */
  // FCG_Fcg0PeriphClockCmd(FCG0_PERIPH_GPIOA, ENABLE);
  FCG_Fcg2PeriphClockCmd(FCG2_PERIPH_TMRA_2, ENABLE);

  stc_gpio_init_t stcGpioInit;
  stc_tmra_init_t stcTmraInit; 

  /* 配置 A、B、SW 引脚 */
  GPIO_StructInit(&stcGpioInit);
  stcGpioInit.u16PinDir = PIN_DIR_IN;
  stcGpioInit.u16PullUp = PIN_PU_ON;
  GPIO_Init(ENC_SW_PORT, ENC_A_PIN | ENC_B_PIN | ENC_SW_PIN, &stcGpioInit);

  /* 配置 A、B 引脚为 TimerA 复用功能 */
  GPIO_SetFunc(ENC_SW_PORT, ENC_A_PIN | ENC_B_PIN, GPIO_FUNC_4);

  /* 配置 TimerA 为编码器正交模式 */
  (void)TMRA_StructInit(&stcTmraInit);
  stcTmraInit.u8CountSrc = TMRA_CNT_SRC_HW;
  stcTmraInit.hw_count.u16CountUpCond = ENC_UP_COND;
  stcTmraInit.hw_count.u16CountDownCond = ENC_DOWN_COND;
  stcTmraInit.u32PeriodValue = 0xFFFF;
  (void)TMRA_Init(ENC_TIMER_UNIT, &stcTmraInit);

  /* 配置输入滤波 */
  TMRA_SetFilterClockDiv(ENC_TIMER_UNIT, TMRA_PIN_CLKA, TMRA_FILTER_CLK_DIV64);
  TMRA_FilterCmd(ENC_TIMER_UNIT, TMRA_PIN_CLKA, ENABLE);
  TMRA_SetFilterClockDiv(ENC_TIMER_UNIT, TMRA_PIN_CLKB, TMRA_FILTER_CLK_DIV64);
  TMRA_FilterCmd(ENC_TIMER_UNIT, TMRA_PIN_CLKB, ENABLE);

  TMRA_Start(ENC_TIMER_UNIT);
  s_u16LastCount = (int16_t)TMRA_GetCountValue(ENC_TIMER_UNIT);

  /* 初始化按键 */
  _encoder_key_init();
}

/* * 获取编码器转动的值，顺时针为正值，逆时针为负值
 * 由 main() 去获取值，获取后去处理，获取时清零（复位），等待下次获取
 */
int8_t encoder_get_count(void) {
  // 读取当前硬件定时器的计数值
  int16_t current_cnt = (int16_t)TMRA_GetCountValue(ENC_TIMER_UNIT);

  int8_t delta = (int8_t)(current_cnt - s_u16LastCount);

  s_u16LastCount = current_cnt;

  return delta;
}

encoder_event_t encoder_event(void) {
  encoder_event_t evt;

  __disable_irq();
  evt = s_eEvent;
  s_eEvent = ENC_SW_NONE;
  __enable_irq();

  return evt;
}
