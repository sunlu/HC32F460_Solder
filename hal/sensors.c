/**
 * @file    sensors.c
 * @brief   Sensor processing - converts ADC raw values to physical quantities
 *
 * ADC reference: AVCC = +3.3V, 12-bit resolution (0-4095)
 * V_per_LSB = 3.3V / 4096 �� 0.8057mV
 */

#include "sensors.h"


#define SENSOR_PORT GPIO_PORT_A
#define REPLACE_PIN GPIO_PIN_03
#define SLEEP_PIN GPIO_PIN_04
#define HANDLE_PIN GPIO_PIN_05
#define SHAKE_PIN GPIO_PIN_09

/* Filter instances */
static t_average_def s_filterTemp;
static t_average_def s_filterTempShow;
static t_average_def s_filterCurrent;
static t_average_def s_filterVoltage;
static t_average_def s_filterPower;
static t_average_def s_filterMCU;

static t_average_def s_filterSleep;
static t_average_def s_filterHandle;
static t_average_def s_filterReplace;
static t_average_def s_filterShake;

static Hysteresis_FilterTypeDef s_hysteresisTemp;

static float g_mcuTemp = 0;

#define THERMOCOUPLE_GAIN 201.0f
#define THERMOCOUPLE_MV_PER_C 0.040f

#define SHUNT_RESISTANCE 0.005f
#define CURRENT_AMP_GAIN 50.0f

#define VOLTAGE_DIVIDER_RATIO 11.25f

static float calc_temp(uint16_t adcVal) {

  if (adcVal > 4090)
    return g_mcuTemp;

  float v_adc = (float)adcVal * ADC_VREF / (float)ADC_RESOLUTION;
  float v_tc_mv = v_adc * 1000.0f / THERMOCOUPLE_GAIN;
  float temp_c = v_tc_mv / THERMOCOUPLE_MV_PER_C;

  /* Simple cold-junction compensation: assume MCU temp �� room temp (25C)
   * In production, read MCU internal temp sensor for better accuracy */
  temp_c += g_mcuTemp;

  return temp_c;
}

static float calc_voltage(uint16_t adcVal) {
  float v_adc = (float)adcVal * ADC_VREF / (float)ADC_RESOLUTION;
  return v_adc * VOLTAGE_DIVIDER_RATIO;
}

static float calc_current(uint16_t adcVal) {
  float v_adc = (float)adcVal * ADC_VREF / (float)ADC_RESOLUTION;

  /* I = V_adc / (SHUNT * GAIN) */
  float current = v_adc / (SHUNT_RESISTANCE * CURRENT_AMP_GAIN);
  if (current < 0.0f)
    current = 0.0f;
  return current;
}

/* Last ADC readings */
static t_adc_val s_adcVal; /* non-static for debug display */

void sensor_gpio_init(void) {
  stc_gpio_init_t stcGpioInit;
  GPIO_StructInit(&stcGpioInit);
  stcGpioInit.u16PinDir = PIN_DIR_IN;
  stcGpioInit.u16PullUp = PIN_PU_ON;

  stcGpioInit.u16PinState = PIN_STAT_SET; /* default high */
  GPIO_Init(SENSOR_PORT, REPLACE_PIN | SLEEP_PIN | HANDLE_PIN | SHAKE_PIN,
            &stcGpioInit);
}

uint8_t sensor_read_pin(uint16_t pin) {
  return GPIO_ReadInputPins(SENSOR_PORT, pin);
}

/* ========================================================================
 * sensors_init - Initialize filters and do initial ADC readings
 * ======================================================================== */
void sensors_init(void) {
  mcu_init();
  adc_init();
  sensor_gpio_init();

  g_mcuTemp = mcu_get_temp();

  average_init(&s_filterTemp, (uint32_t)2);
  average_init(&s_filterTempShow, (uint32_t)50);
  average_init(&s_filterPower, (uint32_t)20);
  average_init(&s_filterMCU, (uint32_t)50);
  average_init(&s_filterVoltage, (uint32_t)25);
  average_init(&s_filterCurrent, (uint32_t)2);

  average_init(&s_filterSleep, (uint32_t)20);
  average_init(&s_filterReplace, (uint32_t)20);
  average_init(&s_filterHandle, (uint32_t)20);
  average_init(&s_filterShake, (uint32_t)20);

  hysteresis_init(&s_hysteresisTemp, 0.5f);
}

/* ========================================================================
 * 高速任务：支架/手柄/功率/温度/按键/紧急检测/烙铁头
 * ======================================================================== */
void sensors_read_hight(void) {
  sensor_val.sleep =
      average_compute(sensor_read_pin(SLEEP_PIN), &s_filterSleep);
  sensor_val.replace =
      average_compute(sensor_read_pin(REPLACE_PIN), &s_filterReplace);
  sensor_val.handle =
      average_compute(sensor_read_pin(HANDLE_PIN), &s_filterHandle);
  sensor_val.shake =
      average_compute(sensor_read_pin(SHAKE_PIN), &s_filterShake);

  if (s_adcVal.C > 4086)
    sensor_val.handleType = HANDLE_NONE;
  else
    sensor_val.handleType = HANDLE_245;

  // sensor_val.handle = (s_adcVal.C <= 4090);
}

//[100ms] 低速任务：电压/电流/MCU温度/到达温度蜂鸣
void sensors_read_low(void) {

  adc_read_all(&s_adcVal);

  sensor_val.temp_last = sensor_val.temp_avg;
  sensor_val.current = calc_current(s_adcVal.A);
  sensor_val.voltage = calc_voltage(s_adcVal.V);
  sensor_val.temp_avg = calc_temp(s_adcVal.C);
  sensor_val.mcu_temp = mcu_get_temp();

  sensor_val.temp_avg = average_compute(sensor_val.temp_avg, &s_filterTemp);
  sensor_val.current = average_compute(sensor_val.current, &s_filterCurrent);
  sensor_val.voltage = average_compute(sensor_val.voltage, &s_filterVoltage);
  sensor_val.power_avg = average_compute(sensor_val.current * sensor_val.voltage, &s_filterPower);
  sensor_val.mcu_temp = average_compute(sensor_val.mcu_temp, &s_filterMCU);

  //
  sensor_val.temp_show =
      average_compute(sensor_val.temp_avg, &s_filterTempShow);
  sensor_val.temp_show =
      hysteresis_add(sensor_val.temp_show, &s_hysteresisTemp);
}

const char *handle_name(HANDLE_TYPE_Def handle) {
  switch (handle) {
  case HANDLE_210:
    return "210";
  case HANDLE_245:
    return "245";
  case HANDLE_470:
    return "470";
  case HANDLE_T12:
    return "T12";
  default:
    return "---";
  }
}
