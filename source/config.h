#ifndef __CONFIG_H
#define __CONFIG_H

#include "hc32_ll.h"

// MCU：HC32F460JETA-LQFP48 (小华半导体, ARM Cortex-M4, 200MHz)
// 开发库：HC32F460_DDL_Rev3.3.0

#include <stdbool.h>

/* ============================================================================
 * 状态机运行状态定义
 * ============================================================================
 */
typedef enum {
  STATE_NONE,
  STATE_RUN,             /* 正常运行状态（加热中） */
  STATE_PRESTANDBY,      /* 预备待机状态（放入烙铁架后的延时阶段） */
  STATE_STANDBY,         /* 待机状态（降低至待机温度） */
  STATE_SLEEP,           /* 休眠状态（加热器完全关闭，烙铁头完全冷却） */
  STATE_EMERGENCY_SLEEP, /* 紧急休眠状态（检测到故障时触发的保护关闭） */
  STATE_HALTED           /* 停机状态（用户手动停止加热） */
} m_state_t;

typedef enum {
  HANDLE_NONE = 0,
  HANDLE_245,
  HANDLE_210,
  HANDLE_T12,
  HANDLE_470
} HANDLE_TYPE_Def;

/* ============================================================================
 * Flash 闪存存储参数结构体
 * ============================================================================
 */
typedef struct {
  float startup_temperature; /* 开机默认设置的目标加热温度 */
  float temperature_offset;  /* 温度校准整体偏移量 */
  float standby_temp;        /* 待机时的目标设定温度 */
  float standby_time;        /* 进入待机的超时时间（单位：分钟） */
  float emergency_time;      /* 紧急关机断电的超时时间（单位：分钟） */
  float buzzer_enabled;      /* 蜂鸣器启用控制（0=关闭，1=开启） */
  float screen_rotation;     /* 屏幕显示旋转角度（0/90/180/270） */
  float momentary_stand;     /* 烙铁架检测模式（点动/常闭切换） */
  float startup_beep;        /* 开机提示音启用控制 */

  float temp_cal_100;                  /* 100℃ 温度校准点 */
  float temp_cal_200;                  /* 200℃ 温度校准点 */
  float temp_cal_300;                  /* 300℃ 温度校准点 */
  float temp_cal_350;                  /* 350℃ 温度校准点 */
  float temp_cal_400;                  /* 400℃ 温度校准点 */
  float temp_cal_450;                  /* 450℃ 温度校准点 */
  float displayed_temp_filter;         /* 屏幕显示温度的滤波系数（滤波强度） */
  float startup_temp_is_previous_temp; /* 开机是否记忆并使用上次的设定温度 */
  float beep_at_set_temp;              /* 温度达到设定值时是否发出提示音 */
  float beep_tone;                     /* 蜂鸣器音调选择 */
  float power_unit;        /* 功率显示单位（0=瓦特 W，1=百分比 %） */
  float display_graph;     /* 是否启用温度/功率曲线图表显示模式 */
  float boost_temp;        /* 强温/功率加速温度（按住按键时临时增加的温度） */
  float boost_time;        /* 强温/功率加速持续时间（单位：秒） */
  float change_enc_dir;    /* 反转旋转编码器的方向 */
  float sleep_timeout_min; /* 进入休眠状态的超时时间（单位：分钟） */
  float standby_delay_s;   /* 进入待机状态的延时时间（单位：秒） */
} t_config_value;

/* ============================================================================
 * 传感器实时采样数据结构体 (由 ADC/传感器处理任务动态更新)
 * ============================================================================
 */
typedef struct {
  float temp_target;     /* PID 控制算法的目标温度 */
 
  float temp_show;
  float temp_last;   /* 上一次采集的热电偶温度 (用于计算温度变化率 delta-T) */
  float temp_avg;    /* 滤波后的热电偶温度 (用于屏幕显示和 PID 闭环控制) */

  float power_req;     /* PID 算法计算出的请求输出功率 */
  float power_avg; /* 滤波后的请求输出功率 */
  float voltage;   /* 输入总线供电电压 (V) */
  float current;   /* 加热器实时电流 (A) */
  float mcu_temp;

  float sleep;
  float replace;
  float handle;
  float shake;

  HANDLE_TYPE_Def handleType; /* 手柄连接状态: ADC>4090=断开, 否则=已连接 */

  m_state_t current_state;  /* 系统当前所处的运行状态 */
  m_state_t previous_state; /* 系统上一次所处的运行状态 */
  float max_power_watt;     /* 当前系统允许的最大功率限制值 */
} t_sensor_value;

/* ============================================================================
 * 全局变量声明 (实际定义在 main.c 中)
 * ============================================================================
 */
extern volatile t_sensor_value sensor_val;
extern t_config_value config_val;
extern const t_config_value config_val_default;

/* 状态机及各任务的时间戳变量 */
extern uint32_t prev_ms_display;
extern uint32_t prev_ms_sensor_high;
extern uint32_t prev_ms_sensor_low;
extern uint32_t prev_ms_standby;
extern uint32_t prev_ms_left_stand;
extern uint32_t prev_ms_prestandby;

/* ============================================================================
 * 系统常量定义配置
 * ============================================================================
 */

#define MIN_SELECTABLE_TEMP 50.0f
#define MAX_SELECTABLE_TEMP 400.0f
#define DEFAULT_TEMP 120.0f // 调试时定60度
#define SLEEP_TEMP 0.0f     /* 当 PID 设定温度为 0 时表示完全关闭加热器 */
#define STANDBY_TEMP_DEFAULT 60.0f

/* 定时任务轮询时间间隔 (单位: ms) */
#define INTERVAL_SENSOR_HIGH 10 /* 高频采样任务：烙铁架、按键、烙铁头拔插检测 \
                                 */
#define INTERVAL_SENSOR_LOW 100 /* 低频采样任务：供电电压、加热电流、MCU 温度 \
                                 */
#define INTERVAL_DISPLAY 50     /* 屏幕刷新显示任务时间间隔 */
#define INTERVAL_CURRENT_MEAS 250 /* 电流脉冲测量任务时间间隔 */

/* 紧急保护触发阈值配置 */
#define EMERGENCY_SHUTDOWN_TEMP 450.0f
#define MIN_BUS_VOLTAGE 8.0f
#define MAX_TC_DELTA_FAULT 50.0f

/* ============================================================================
 * millis() - 获取系统当前的运行时间戳
 * (基于软件 SysTick 滴答定时器实现，系统基础分辨率为 10ms)
 * ============================================================================
 */
extern volatile uint32_t g_system_tick;
#define millis() (g_system_tick * 10U)

//==============================================================================
// 微控制器 (MCU)
//==============================================================================
#define HXTAL_FREQ 16000000UL /* 外部16MHz晶振 */
#define CPU_FREQ 200000000UL

/* T245 烙铁头 PID 参数（与原版固件一致） */
#define PID_MAX_OUTPUT 500.0f
#define PID_UPDATE_INTERVAL_MS 25
#define PID_I_MIN_ERROR 75.0f

#define PID_KP 8.0f
#define PID_KI 2.0f
#define PID_KD 0.5f
#define PID_MAX_I 300.0f
#define PID_NEG_ERROR_I_MULT 7.0f
#define PID_NEG_ERROR_I_BIAS 1.0f
// #define PID_SAMPLE_TIME_MS      100

//==============================================================================
// 功率转换 (AxxSolder 公式: duty = pid_output * max_watt * FACTOR / voltage)
// 0.246 = 0.123 * 1000/500 (适配 duty_x10 0~1000 量纲)
//==============================================================================
#define POWER_CONVERSION_FACTOR 0.246f

//==============================================================================
// 温度限制
//==============================================================================
#define TEMP_MAX 450
#define TEMP_MIN 150
#define TEMP_DEFAULT 320
#define TEMP_SLEEP 100
#define TEMP_OVERHEAT 500

//==============================================================================
// 休眠/待机定时器
//==============================================================================
#define SLEEP_TIMEOUT_MS 180000
#define STANDBY_TIMEOUT_MS 600000

//==============================================================================
// 系统时钟分频
//==============================================================================
#define TICK_1MS 1
#define TICK_10MS 10
#define TICK_50MS 50
#define TICK_100MS 100
#define TICK_500MS 500
#define TICK_1000MS 1000

#endif
