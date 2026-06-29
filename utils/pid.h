/**
 * @file    pid.h
 * @brief   PID控制器头文件
 *
 * 本模块实现了标准的PID（比例-积分-微分）控制算法，包含以下特性：
 * - 自动/手动模式切换
 * - 正/反向控制方向
 * - 抗积分饱和（积分限幅 + 条件积分）
 * - 非对称积分增益（正负误差使用不同的I增益）
 * - 测量值微分（微分项基于输入变化而非误差变化，避免设定点突变冲击）
 * - 输出限幅
 */
#ifndef INC_PID_H
#define INC_PID_H

/* 包含文件 */
#include <stdint.h>
#include <string.h>
#include "main.h"
#include "hc32_ll.h"
#include <math.h>

#define DEFAULT_SAMPLE_TIME_MS 100   // 默认PID采样时间（毫秒）
#define DEFAULT_PWM_MAX 100          // 默认PWM最大输出

/* PID工作模式 */
typedef enum{
    PID_MODE_MANUAL    = 0,  // 手动模式（PID不计算）
    PID_MODE_AUTOMATIC = 1   // 自动模式（PID正常计算）
}PIDMode_TypeDef;

/* PID控制方向 */
typedef enum{
    PID_CD_DIRECT  = 0,   // 正作用（输出随输入增大而增大）
    PID_CD_REVERSE = 1    // 反作用（输出随输入增大而减小）
}PIDCD_TypeDef;

/**
 * @brief PID控制器结构体
 *
 * 注：Kp/Ki/Kd 为内部计算用的实际增益值（可能为负值），
 *     DispKp/DispKi/DispKd 为用户设置的原始值（始终为正值）。
 *     当方向为REVERSE时，实际值为原始值的相反数。
 */
typedef struct{

    PIDMode_TypeDef InAuto;                   // 自动/手动模式标志
    PIDCD_TypeDef   ControllerDirection;       // 控制方向

    uint32_t        LastTime;                  // 上次PID计算的时间戳（HAL_GetTick()）
    uint32_t        SampleTime;                // PID采样时间间隔（毫秒）

    uint32_t        updateOnEveryCall;         // 是否每次调用都计算（忽略SampleTime）

    float          DispKp;                     // 显示/用户设置的Kp值（正值）
    float          DispKi;                     // 显示/用户设置的Ki值
    float          DispKd;                     // 显示/用户设置的Kd值

    float          Kp;                         // 实际内部Kp值（根据方向可能为负）
    float          Ki;                         // 实际内部Ki值
    float          Kd;                         // 实际内部Kd值

    float          DispKp_part;               // 当前P项贡献（用于显示/调试）
    float          DispKi_part;               // 当前I项贡献
    float          DispKd_part;               // 当前D项贡献

    volatile float *MyInput;                   // 指向PID输入（测量值）的指针
    volatile float *MyOutput;                  // 指向PID输出的指针
    volatile float *MySetpoint;               // 指向PID设定值的指针

    float          OutputSum;                 // 积分累加器
    float          LastInput;                 // 上次输入值（用于计算微分项dInput）

    float          OutMin;                    // 输出下限
    float          OutMax;                    // 输出上限

    float          IMin;                       // 积分累加器下限
    float          IMax;                       // 积分累加器上限

    float          IminError;                  // 最小误差阈值：误差大于此值时积分项清零

    float          NegativeErrorIgainMultiplier; // 负误差I增益乘数（非对称积分：负误差的I增益系数）
    float          NegativeErrorIgainBias;       // 负误差I增益偏移

}PID_TypeDef;

/* 初始化 */

/**
 * @brief 初始化PID控制器的内部状态（设置LastInput=当前输入，clamp积分累加器）
 */
void PID_Init(PID_TypeDef *uPID);

/**
 * @brief 设置PID控制器的基本参数并初始化
 * @param uPID              PID结构体指针
 * @param Input             输入指针
 * @param Output            输出指针
 * @param Setpoint          设定值指针
 * @param Kp, Ki, Kd        比例/积分/微分增益
 * @param ControllerDirection 控制方向
 */
void PID_Config(PID_TypeDef *uPID, volatile float *Input, volatile float *Output, volatile float *Setpoint, float Kp, float Ki, float Kd, PIDCD_TypeDef ControllerDirection);

/* 工具函数 */

/**
 * @brief 浮点数clamp（限制在[min, max]范围内）
 */
float float_clamp(float d, float min, float max);

/**
 * @brief 检查浮点数是否会触发clamp（超出[min, max]范围）
 * @return 1=会触发clamp, 0=在范围内
 */
uint8_t check_clamping(float d, float min, float max);

/* PID计算 */

/**
 * @brief 执行一次PID计算（如果未达到采样时间则跳过）
 * @return 1=已计算, 0=因时间未到而跳过
 */
uint8_t PID_Compute(PID_TypeDef *uPID);

/* PID限幅设置 */

/**
 * @brief 设置PID输出限幅
 */
void PID_SetOutputLimits(PID_TypeDef *uPID, float Min, float Max);

/**
 * @brief 设置积分累加器限幅（防止积分饱和）
 */
void PID_SetILimits(PID_TypeDef *uPID, float Min, float Max);

/**
 * @brief 设置最小误差阈值 - 当|error|>IminError时清除积分累加器
 */
void PID_SetIminError(PID_TypeDef *uPID, float IminError);

/**
 * @brief 设置负误差时的I增益系数（非对称积分）
 * @param NegativeErrorIgainMultiplier 负误差I增益乘数
 * @param NegativeErrorIgainBias       负误差I增益偏移
 */
void PID_SetNegativeErrorIgainMult(PID_TypeDef *uPID, float NegativeErrorIgainMultiplier, float NegativeErrorIgainBias);

/* PID参数设置 */

/**
 * @brief 设置PID增益参数（Kp, Ki, Kd）
 */
void PID_SetTunings(PID_TypeDef *uPID, float Kp, float Ki, float Kd);

/* PID方向设置 */

/**
 * @brief 设置PID控制方向（切换时会反号增益值）
 */
void          PID_SetControllerDirection(PID_TypeDef *uPID, PIDCD_TypeDef Direction);
PIDCD_TypeDef PID_GetDirection(PID_TypeDef *uPID);

/* PID采样时间 */

/**
 * @brief 设置PID采样时间
 * @param NewSampleTime 采样间隔（毫秒）
 * @param updateOnCall  非0时每次调用PID_Compute都计算（忽略采样时间）
 */
void PID_SetSampleTime(PID_TypeDef *uPID, int32_t NewSampleTime, int32_t updateOnCall);

/**
 * @brief 设置PID模式（自动/手动切换时调用PID_Init初始化状态）
 */
void PID_SetMode(PID_TypeDef *uPID, PIDMode_TypeDef Mode);

/* 获取PID参数 */

float PID_GetKp(PID_TypeDef *uPID);
float PID_GetKi(PID_TypeDef *uPID);
float PID_GetKd(PID_TypeDef *uPID);

/* 获取当前PID各项贡献 */

float PID_GetPpart(PID_TypeDef *uPID);    // 获取P项（比例项）当前贡献
float PID_GetIpart(PID_TypeDef *uPID);    // 获取I项（积分项）当前贡献
float PID_GetDpart(PID_TypeDef *uPID);    // 获取D项（微分项）当前贡献

#endif
