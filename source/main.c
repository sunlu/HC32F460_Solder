/**
 * @file    main.c
 * @brief   JBC245 Soldering Station - Main Application (ported from AxxSolder
 * v3.x)
 *
 * MCU: HC32F460JETA-LQFP48 (Cortex-M4, 200MHz, 512KB Flash, 192KB RAM)
 *
 * Main loop timing (bare-metal cooperative multitasking, 10ms base tick):
 *   10ms:  High-priority (sensors, state machine, emergency check, encoder,
 * PID) 20ms:  Display update (AxxSolder original: 15ms) 100ms: Low-priority
 * (temp-reached beep, popup timeout)
 *
 * Key ports from AxxSolder:
 *   1. Power conversion formula: duty = pid_output * max_watt * 0.246 / bus_v
 *      Prevents 100% duty surge on cold start
 *   2. PID params per handle type (T245: Kp=8 Ki=2 Kd=0.5 Max_I=300)
 *   3. Asymmetric I-gain: I-term * 7 during overshoot for fast recovery
 */

#include "main.h"
#include "buzzer.h"
#include "config.h"
#include "encoder.h"
#include "gui.h"
#include "heater.h"
#include "lcd.h"

#include "menu.h"
#include "pid.h"
#include "sensors.h"
#include "state.h"
#include "storage.h"
#include <stdio.h>
#include <string.h>

/* Sensor values (updated by sensors_read() each 10ms) */
volatile t_sensor_value sensor_val = {.temp_target = DEFAULT_TEMP,
                                      .temp_show = 25.0f,

                                      .temp_last = 25.0f,
                                      .temp_avg = 25.0f,

                                      .power_req = 0.0f,
                                      .power_avg = 0.0f,
                                      .voltage = 24.0f,
                                      .current = 0.0f,
                                      .mcu_temp = 0.0f,

                                      .sleep = 0.0f,
                                      .replace = 0.0f,
                                      .handle = 0.0f,
                                      .shake = 0.0f,
                                      .handleType = HANDLE_NONE,
                                      .current_state = STATE_SLEEP,
                                      .previous_state = STATE_SLEEP,
                                      .max_power_watt = 70.0f};

/* Flash config (defaults used until storage_init() is ready) */
t_config_value config_val;
const t_config_value config_val_default = {
    .startup_temperature = DEFAULT_TEMP,
    .temperature_offset = 0.0f,
    .standby_temp = STANDBY_TEMP_DEFAULT,
    .standby_time = 10.0f,
    .emergency_time = 30.0f,
    .buzzer_enabled = 1.0f,
    .screen_rotation = 2.0f,
    .momentary_stand = 0.0f,
    .startup_beep = 1.0f,
    .temp_cal_100 = 100.0f,
    .temp_cal_200 = 200.0f,
    .temp_cal_300 = 300.0f,
    .temp_cal_350 = 350.0f,
    .temp_cal_400 = 400.0f,
    .temp_cal_450 = 450.0f,
    .displayed_temp_filter = 5.0f,
    .startup_temp_is_previous_temp = 0.0f,
    .beep_at_set_temp = 1.0f,
    .beep_tone = 0.0f,
    .power_unit = 1.0f,
    .display_graph = 0.0f,
    .boost_temp = 400.0f,
    .boost_time = 30.0f,
    .change_enc_dir = 0.0f,
    .sleep_timeout_min = 30.0f,
    .standby_delay_s = 5.0f,
};

/* Display state */
uint8_t menu_active = 0;
uint8_t popup_shown = 0;
uint32_t popup_start_ms = 0;
char DISPLAY_buffer[40];

/* Software tick counter (incremented every 10ms, drives millis() macro) */
volatile uint32_t g_system_tick = 0;

uint32_t prev_ms_display = 0;
uint32_t prev_ms_sensor_high = 0;
uint32_t prev_ms_sensor_low = 0;
uint32_t prev_ms_standby = 0;
uint32_t prev_ms_left_stand = 0;
uint32_t prev_ms_prestandby = 0;

/* ============================================================================
 * PID Controller Instance
 * ============================================================================
 */
static PID_TypeDef TPID;

/* ============================================================================
 * Utility Functions
 * ============================================================================
 */

/** @brief Clamp float value to [min, max] */
float clamp_float(float d, float min, float max) {
  const float t = d < min ? min : d;
  return t > max ? max : t;
}

/* ============================================================================
 * System Clock & SysTick
 * ============================================================================
 */

/**
 * @brief System clock: 16MHz XTAL -> PLL -> 200MHz
 *
 * Bus dividers:
 *   HCLK=200M, EXCLK=100M, PCLK0=200M, PCLK1=100M,
 *   PCLK2=50M, PCLK3=50M, PCLK4=100M
 */
void bsp_clk_init(void) {
  CLK_SetClockDiv(CLK_BUS_CLK_ALL, (CLK_HCLK_DIV1 | CLK_EXCLK_DIV2 | CLK_PCLK0_DIV1 | CLK_PCLK1_DIV2 | CLK_PCLK2_DIV4 |
                                    CLK_PCLK3_DIV4 | CLK_PCLK4_DIV2));

  GPIO_AnalogCmd(GPIO_PORT_H, GPIO_PIN_00 | GPIO_PIN_01, ENABLE);

  SRAM_SetWaitCycle(SRAM_SRAM_ALL, SRAM_WAIT_CYCLE1, SRAM_WAIT_CYCLE1);
  SRAM_SetWaitCycle(SRAM_SRAMH, SRAM_WAIT_CYCLE0, SRAM_WAIT_CYCLE0);

  EFM_SetWaitCycle(EFM_WAIT_CYCLE5);

  stc_clock_xtal_init_t stcXtalInit;
  (void)CLK_XtalStructInit(&stcXtalInit);
  stcXtalInit.u8State = CLK_XTAL_ON;
  stcXtalInit.u8Drv = CLK_XTAL_DRV_HIGH;
  stcXtalInit.u8Mode = CLK_XTAL_MD_OSC;
  stcXtalInit.u8StableTime = CLK_XTAL_STB_16MS;
  (void)CLK_XtalInit(&stcXtalInit);

  stc_clock_pll_init_t stcMPLLInit;
  (void)CLK_PLLStructInit(&stcMPLLInit);
  stcMPLLInit.PLLCFGR = 0UL;
  stcMPLLInit.PLLCFGR_f.PLLM = (2UL - 1UL);
  stcMPLLInit.PLLCFGR_f.PLLN = (50UL - 1UL);
  stcMPLLInit.PLLCFGR_f.PLLP = (2UL - 1UL);
  stcMPLLInit.PLLCFGR_f.PLLQ = (2UL - 1UL);
  stcMPLLInit.PLLCFGR_f.PLLR = (2UL - 1UL);
  stcMPLLInit.u8PLLState = CLK_PLL_ON;
  stcMPLLInit.PLLCFGR_f.PLLSRC = CLK_PLL_SRC_XTAL;
  (void)CLK_PLLInit(&stcMPLLInit);

  GPIO_SetReadWaitCycle(GPIO_RD_WAIT3);
  PWC_HighSpeedToHighPerformance();
  CLK_SetSysClockSrc(CLK_SYSCLK_SRC_PLL);
}

/** @brief Init SysTick at 1kHz (1ms resolution) */
void sys_tick_init(void) { SysTick_Init(1000); }

/** @brief SysTick ISR: increment 1ms counter */
void SysTick_Handler(void) { SysTick_IncTick(); }

/* ============================================================================
 * PID Initialization
 * ============================================================================
 */

/**
 * @brief Initialize PID controller for T245 cartridge
 *
 * Default T245 params: Kp=8 Ki=2 Kd=0.5 Max_I=300 IminError=75
 * NegativeErrorIgainMult=7: during overshoot, I-term decays 7x faster
 *
 * Input:  sensor_val.temp_avg (filtered thermocouple temp, 0-500C)
 * Output: pid_output (0-500)
 * Setpoint: PID_setpoint (managed by state machine)
 */
static void pid_init(void) {
  PID_Config(&TPID, (volatile float *)&sensor_val.temp_avg, /* 滤波后温度 */
             &sensor_val.power_req, &PID_setpoint,          /* 状态机管理 */
             PID_KP, PID_KI, PID_KD, PID_CD_DIRECT);

  PID_SetMode(&TPID, PID_MODE_AUTOMATIC);
  PID_SetSampleTime(&TPID, PID_UPDATE_INTERVAL_MS, 0);
  PID_SetOutputLimits(&TPID, 0, PID_MAX_OUTPUT);
  PID_SetILimits(&TPID, -PID_MAX_I, PID_MAX_I); /* 对称积分限幅 */
  PID_SetIminError(&TPID, PID_I_MIN_ERROR);
  PID_SetNegativeErrorIgainMult(&TPID, PID_NEG_ERROR_I_MULT, PID_NEG_ERROR_I_BIAS);
}

/* ============================================================================
 * Encoder Input Handler
 * ============================================================================
 */

/**
 * @brief Process rotary encoder input
 *
 * Rotation: adjust set temp in 5C steps, wake from SLEEP
 * Short press: toggle RUN <-> HALTED, or SLEEP -> RUN
 * Long press: enter settings menu
 */
static void handle_encoder_input(void) {
  encoder_event_t evt = encoder_event();
  int8_t rotation = encoder_get_count();

  /* Rotation: adjust temperature + buzzer feedback */
  if (rotation != 0) {
    float t = sensor_val.temp_target + rotation * 5.0f;
    t = clamp_float(t, MIN_SELECTABLE_TEMP, MAX_SELECTABLE_TEMP);
    sensor_val.temp_target = t;
    buzzer_trigger(BUZZ_ONCE);

    if (sensor_val.current_state == STATE_SLEEP) {
      change_state(STATE_RUN);
    }
  }

  /* Short press: toggle RUN/HALT */
  if (evt == ENC_SW_SHORT_PRESS) {
    if (sensor_val.current_state == STATE_RUN || sensor_val.current_state == STATE_PRESTANDBY ||
        sensor_val.current_state == STATE_STANDBY) {
      change_state(STATE_HALTED);
    } else if (sensor_val.current_state == STATE_HALTED || sensor_val.current_state == STATE_SLEEP) {
      change_state(STATE_RUN);
    }
  }

  /* Long press: enter settings menu */
  if (evt == ENC_SW_LONG_PRESS) {
    menu_enter();
  }
}

/* ============================================================================
 * Temperature Reached Beep
 * ============================================================================
 */

static uint8_t s_beeped_at_temp = 0;

/** @brief Double-beep when temperature reaches setpoint */
static void handle_temp_reached_beep(void) {
  float error = sensor_val.temp_target - sensor_val.temp_avg;

  if (sensor_val.current_state == STATE_RUN && config_val.beep_at_set_temp > 0.5f) {
    if (error < 5.0f && error > -5.0f && !s_beeped_at_temp) {
      buzzer_trigger(BUZZ_DOUBLE);
      s_beeped_at_temp = 1;
    }
    if (error > 10.0f || error < -10.0f) {
      s_beeped_at_temp = 0;
    }
  }
}

/* ============================================================================
 * Popup Timeout
 * ============================================================================
 */

/** @brief Auto-dismiss popup after 2 seconds */
static void handle_popup_timeout(void) {
  if (popup_shown && (millis() - popup_start_ms > 2000)) {
    popup_shown = 0;
    gui_draw_main_screen();
  }
}

/* ============================================================================
 * Main Entry Point
 * ============================================================================
 */

int32_t main(void) {
  /* ---- Phase 0: Unlock all peripheral registers ---- */
  LL_PERIPH_WE(LL_PERIPH_ALL);

  /* ---- Phase 1: System clock (16MHz XTAL -> PLL -> 200MHz) ---- */
  bsp_clk_init();

  /* ---- Phase 2: SysTick (1ms timebase) ---- */
  sys_tick_init();

  /* ---- Phase 3: Peripheral init ---- */
  lcd_init();
  buzzer_init();
  sensors_init(); /* ADC init inside */
  mcu_init();
  heater_init();

  /* ---- Phase 4: Module init ---- */
  encoder_init();
  pid_init();
  gui_init();

  /* ---- Phase 5: Load config (defaults until Flash storage ready) ---- */
  memcpy(&config_val, &config_val_default, sizeof(t_config_value));
  /* storage_init(); */ /* TODO: enable when Flash storage ready */

  /* ---- Phase 6: Lock peripheral registers ---- */
  LL_PERIPH_WP(LL_PERIPH_ALL);

  /* Startup beep */
  buzzer_trigger(BUZZ_BOOT);

  /* Start in SLEEP state, wait for user action */
  state_init();

  /* Draw main screen */
  gui_draw_main_screen();

  /* ========================================================================
   * Main Loop — cooperative scheduler based on DDL_DelayMS(10)
   *
   * Timing (adapted from AxxSolder):
   *   Every tick (10ms):  sensors, state machine, emergency check,
   *                        encoder, PID control
   *   Every 2 ticks (20ms): display update (AxxSolder: 15ms)
   *   Every 10 ticks (100ms): beep check, popup timeout
   * ======================================================================== */

  for (uint8_t i = 0; i < 200; i++) {
    sensors_read_hight();
    sensors_read_low();
  }

  uint32_t tick = 0;

  sensor_val.max_power_watt = 60;

  for (;;) {

    DDL_DelayMS(10); /* base tick = 10ms */
    tick++;
    g_system_tick = tick;
    sensors_read_hight();

    if ((tick % 10) == 0) {
      sensors_read_low();
    }

    state_update();          /* state machine (stand/sleep logic) */
    state_emergency_check(); /* fault detection (overtemp/undervolt) */
    handle_encoder_input();  /* encoder rotation + buttons */

    /* ---- PID Heating Control (AxxSolder power formula) ---- */
    {
      m_state_t st = sensor_val.current_state;
      uint8_t is_heating = (st == STATE_RUN || st == STATE_PRESTANDBY || st == STATE_STANDBY);

      if (is_heating) {

        /* PID 无条件调用，内部管理 25ms 时序 */
        PID_Compute(&TPID);

        uint16_t duty = (uint16_t)(sensor_val.power_req * (sensor_val.max_power_watt * 0.123 / sensor_val.voltage)) / 5;

        heater_set_duty(duty);
      } else {
        heater_off();
      }
    }

    /* ================================================================
     * Every 100ms (10 ticks): Low-priority tasks
     * ================================================================ */
    if ((tick % 10) == 0) {
      handle_temp_reached_beep();
      handle_popup_timeout();
    }

    /* ================================================================
     * Every 500ms (50 ticks): Display update
     * ================================================================ */
    if ((tick % 20) == 0) {
      if (!popup_shown) {
        gui_update_display();
      }
    }
  }
}

/* ============================================================================
 * Error Handler
 * ============================================================================
 */

/** @brief Hardware fault callback: disable IRQ, kill heater, alarm beep */
void Error_Handler(void) {
  __disable_irq();
  heater_off();
  while (1) {
    buzzer_trigger(BUZZ_DOUBLE);
  }
}
