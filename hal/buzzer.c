/**
 * @file    buzzer.c
 * @brief   小华 HC32F460 智能非阻塞蜂鸣器驱动
 * PWM音频输出: PB8 (TMRA_4_CH3)
 * 自动倒计时/状态机驱动: TMRA_3 (10ms 中断)
 */

#include "buzzer.h"

#define BUZZER_PORT           GPIO_PORT_B
#define BUZZER_PIN            GPIO_PIN_08
#define BUZZER_PWM_UNIT       CM_TMRA_4
#define BUZZER_PWM_CH         TMRA_CH3

#define BUZZER_PWM_PERIOD     2313U  //PWM频率 = PCLK / (预分频系数 × (周期值 + 1)) 周期值 = 50,000,000 / (预分频 × 2700) - 1
#define BUZZER_PWM_COMPARE    1156U  
 
#define AUTO_DRIVE_TIMER      CM_TMRA_3          // 由   CM_TMRA_3
#define AUTO_DRIVE_FCG        FCG2_PERIPH_TMRA_3 // 改为 TMRA_3 的时钟使能宏
#define AUTO_DRIVE_IRQ        INT_SRC_TMRA_3_OVF // 改为 TMRA_3 的溢出中断源
#define AUTO_DRIVE_INT_NUM    INT020_IRQn        // 建议换一个中断向量号（如 INT002），防止与编码器冲突
#define AUTO_DRIVE_PERIOD     7811U               // 10ms 周期 (50MHz, 64分频)

// 内部状态机私有变量
static volatile buzz_mode_t s_eMode = BUZZ_OFF;
static volatile uint16_t s_u16CountTick = 0;      // 倒计时 Tick
static volatile uint8_t  s_u8Phase = 0;           // 复合音阶段
static uint16_t s_u16ConfiguredTicks = 10;        // 默认单次鸣叫持续 100ms (10*10ms)

static void buzzer_hw_on(void);
static void buzzer_hw_off(void);
static void buzzer_internal_update(void);

/* ====================================================================
 * buzzer_init - 硬件全自动化初始化
 * ==================================================================== */
void buzzer_init(void) {
    stc_tmra_init_t stcTmraInit;
    stc_gpio_init_t stcGpioInit;
    stc_tmra_pwm_init_t stcPwmInit;
    stc_irq_signin_config_t stcIrqSign;

    // 1. 初始化 PWM 引脚及定时器时钟 (默认配置为 2.7kHz 谐振点)
    //PWM频率 = PCLK / (预分频系数 × (周期值 + 1))
    FCG_Fcg2PeriphClockCmd(FCG2_PERIPH_TMRA_4, ENABLE);
   

    GPIO_StructInit(&stcGpioInit);
    stcGpioInit.u16PinDir = PIN_DIR_OUT;
    stcGpioInit.u16PinOutputType = PIN_OUT_TYPE_CMOS;
    GPIO_Init(BUZZER_PORT, BUZZER_PIN, &stcGpioInit);
    GPIO_ResetPins(BUZZER_PORT, BUZZER_PIN);

    (void)TMRA_StructInit(&stcTmraInit);
    stcTmraInit.u8CountSrc = TMRA_CNT_SRC_SW; 
    stcTmraInit.sw_count.u8CountMode = TMRA_MD_SAWTOOTH;
    stcTmraInit.sw_count.u8CountDir = TMRA_DIR_UP;
    stcTmraInit.sw_count.u8ClockDiv = TMRA_CLK_DIV8;
    stcTmraInit.u32PeriodValue = BUZZER_PWM_PERIOD; // 2.7kHz 初始周期
    (void)TMRA_Init(BUZZER_PWM_UNIT, &stcTmraInit);

    (void)TMRA_PWM_StructInit(&stcPwmInit);
    stcPwmInit.u32CompareValue = BUZZER_PWM_COMPARE; // 50% 占空比
    stcPwmInit.u16StartPolarity = TMRA_PWM_HIGH;         
    stcPwmInit.u16StopPolarity = TMRA_PWM_LOW;           
    stcPwmInit.u16CompareMatchPolarity = TMRA_PWM_LOW;   
    stcPwmInit.u16PeriodMatchPolarity = TMRA_PWM_HIGH;   
    (void)TMRA_PWM_Init(BUZZER_PWM_UNIT, BUZZER_PWM_CH, &stcPwmInit);

    GPIO_SetFunc(BUZZER_PORT, BUZZER_PIN, GPIO_FUNC_4);
    
    buzzer_hw_off();

    // 2. 初始化 10ms 自动脱机驱动定时器
    FCG_Fcg2PeriphClockCmd(AUTO_DRIVE_FCG, ENABLE);
    stcTmraInit.sw_count.u8ClockDiv = TMRA_CLK_DIV64;   
    stcTmraInit.u32PeriodValue = AUTO_DRIVE_PERIOD;
    (void)TMRA_Init(AUTO_DRIVE_TIMER, &stcTmraInit);

    stcIrqSign.enIntSrc = AUTO_DRIVE_IRQ;
    stcIrqSign.enIRQn = AUTO_DRIVE_INT_NUM;
    stcIrqSign.pfnCallback = &buzzer_internal_update;
    (void)INTC_IrqSignIn(&stcIrqSign);

    TMRA_IntCmd(AUTO_DRIVE_TIMER, TMRA_INT_OVF, ENABLE);
    NVIC_ClearPendingIRQ(stcIrqSign.enIRQn);
    NVIC_SetPriority(stcIrqSign.enIRQn, DDL_IRQ_PRIO_DEFAULT); 
    NVIC_EnableIRQ(stcIrqSign.enIRQn);

    TMRA_Start(AUTO_DRIVE_TIMER); // 开启后台自动看门狗机制
}

/* ====================================================================
 * buzzer_set_tone - 终极修复版：彻底切断同步陷阱
 * @param frequency_hz: 频率 (例如 1500)
 * @param duration_ms:  持续时间 (例如 50)
 * ==================================================================== */
void buzzer_set_tone(uint32_t frequency_hz, uint32_t duration_ms) {
    if (frequency_hz == 0) return;

    // 1. 彻底关闭定时器和 PWM 输出通道使能，防止华大内部硬件处于“忙”状态而锁死缓冲器
    TMRA_Stop(BUZZER_PWM_UNIT);
    TMRA_PWM_OutputCmd(BUZZER_PWM_UNIT, BUZZER_PWM_CH, DISABLE);

    // 2. 重新计算 50MHz 下的周期和比较值
    uint32_t u32Period = (50000000UL / 8 / frequency_hz) - 1;
    uint32_t u32Compare = u32Period / 2U;

    // 3. 强行写入新的参数（部分 DDL 在定时器 Stop 状态下写这两个函数，会直接灌入影子寄存器）
    TMRA_SetPeriodValue(BUZZER_PWM_UNIT, u32Period);
    TMRA_SetCompareValue(BUZZER_PWM_UNIT, BUZZER_PWM_CH, u32Compare);

    // 4. 【关键核心】强行清零计数器。华大 TimerA 只有在 Count 重新从 0 开始时，
    // 并且下一次启动后，才会真正触发比较波形的翻转硬件逻辑！
    TMRA_SetCountValue(BUZZER_PWM_UNIT, 0U);

    // 5. 将毫秒转换为 10ms 状态机的 Ticks
    s_u16ConfiguredTicks = (uint16_t)(duration_ms / 10U);
    if (s_u16ConfiguredTicks == 0) {
        s_u16ConfiguredTicks = 1; // 至少响 1 个 Tick (10ms)
    }
}

/* ====================================================================
 * buzzer_trigger - 触发鸣叫（非阻塞接口，100% 安全）
 * ==================================================================== */
void buzzer_trigger(buzz_mode_t mode) {
    __disable_irq(); // 涉及中断状态机修改，加锁保护

    buzzer_hw_off();
    s_eMode = mode;
    s_u16CountTick = 0;
    s_u8Phase = 0;

    __enable_irq();
}

/* 硬件底层私有控制 */
static void buzzer_hw_on(void) {
    TMRA_PWM_OutputCmd(BUZZER_PWM_UNIT, BUZZER_PWM_CH, ENABLE);
    TMRA_Start(BUZZER_PWM_UNIT);
}

static void buzzer_hw_off(void) {
    TMRA_PWM_OutputCmd(BUZZER_PWM_UNIT, BUZZER_PWM_CH, DISABLE);
    TMRA_Stop(BUZZER_PWM_UNIT);
    TMRA_SetCountValue(BUZZER_PWM_UNIT, 0U);
}

/* ====================================================================
 * buzzer_internal_update - 10ms 中断内运行的“自销毁状态机”
 * ==================================================================== */
static void buzzer_internal_update(void) {
    TMRA_ClearStatus(AUTO_DRIVE_TIMER, TMRA_FLAG_OVF);

    if (s_eMode == BUZZ_OFF) return;

    s_u16CountTick++;

    switch (s_eMode) {
        case BUZZ_ONCE: // 类似 STM32 的单次硬件自定义发声
            if (s_u16CountTick == 1U) {
                buzzer_hw_on();
            } else if (s_u16CountTick >= s_u16ConfiguredTicks) {
                buzzer_hw_off();
                s_eMode = BUZZ_OFF;
            }
            break;

        case BUZZ_DOUBLE: // 完美解决 STM32 Delay(100) 的非阻塞双击音
            switch (s_u8Phase) {
                case 0: // 第一次响 50ms
                    if (s_u16CountTick == 1U) buzzer_hw_on();
                    else if (s_u16CountTick >= 5U) { // 5 * 10ms = 50ms
                        buzzer_hw_off();
                        s_u8Phase = 1;
                        s_u16CountTick = 0;
                    }
                    break;
                case 1: // 停 100ms
                    if (s_u16CountTick >= 10U) { // 10 * 10ms = 100ms
                        s_u8Phase = 2;
                        s_u16CountTick = 0;
                    }
                    break;
                case 2: // 第二次响 50ms
                    if (s_u16CountTick == 1U) buzzer_hw_on();
                    else if (s_u16CountTick >= 5U) {
                        buzzer_hw_off();
                        s_eMode = BUZZ_OFF;
                    }
                    break;
            }
            break;

        case BUZZ_BOOT: // 开机音乐
            switch (s_u8Phase) {
                case 0: if (s_u16CountTick == 1U) buzzer_hw_on();
                        else if (s_u16CountTick >= 5U) { buzzer_hw_off(); s_u8Phase = 1; s_u16CountTick = 0; }
                        break;
                case 1: if (s_u16CountTick >= 10U) { s_u8Phase = 2; s_u16CountTick = 0; } break;
                case 2: if (s_u16CountTick == 1U) buzzer_hw_on();
                        else if (s_u16CountTick >= 5U) { buzzer_hw_off(); s_u8Phase = 3; s_u16CountTick = 0; }
                        break;
                case 3: if (s_u16CountTick >= 10U) { s_u8Phase = 4; s_u16CountTick = 0; } break;
                case 4: if (s_u16CountTick == 1U) buzzer_hw_on();
                        else if (s_u16CountTick >= 5U) { buzzer_hw_off(); s_eMode = BUZZ_OFF; }
                        break;
            }
            break;

        default:
            buzzer_hw_off();
            s_eMode = BUZZ_OFF;
            break;
    }
}
