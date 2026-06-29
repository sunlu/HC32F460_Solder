/**
 * @file    pid.c
 * @brief   PID控制器实现
 *
 * 本PID实现基于 Brett Beauregard 的 Arduino PID 库 v1.2.1，
 * 主要改进：
 *
 * 1. 时序安全：使用HAL_GetTick()获取实际时间，以seconds为单位计算
 *    Ki和Kd，使增益值与采样时间无关。
 *
 * 2. 非对称I增益：对于负误差（温度过冲时），使用更小的I增益
 *    (NegativeErrorIgainMultiplier)，以减少降温过程中的积分累积，
 *    加快从过冲恢复的速度。
 *
 * 3. 测量值微分：微分项基于传感器输入的变化（dInput）而非误差的
 *    变化（dError），避免设定点突变引起的微分冲击（Derivative Kick）。
 *
 * 4. 条件积分（抗饱和）：如果输出已经饱和且增加积分项只会加大饱和，
 *    则暂停积分累加。此外，当误差大于IminError时清零积分项，
 *    实现积分分离。
 *
 * 5. 比例项基于误差（P on Error）：采用标准的比例-误差模式。
 */
#include "pid.h"

/**
 * @brief 初始化PID控制器内部状态
 * 设置积分累加器为当前输出值（clamp到输出限幅内），
 * 记录当前输入作为LastInput。
 */
void PID_Init(PID_TypeDef *uPID)
{
    uPID->OutputSum = *uPID->MyOutput;
    uPID->LastInput = *uPID->MyInput;

    uPID->OutputSum = float_clamp(uPID->OutputSum, uPID->OutMin, uPID->OutMax);
}

/**
 * @brief 配置PID控制器的基本参数（初始化或重新配置）
 *
 * @param uPID              PID结构体
 * @param Input             测量值指针
 * @param Output            输出值指针
 * @param Setpoint          设定值指针
 * @param Kp, Ki, Kd        增益参数
 * @param ControllerDirection 控制方向
 */
void PID_Config(PID_TypeDef *uPID,
		volatile float *Input,
		volatile float *Output,
		volatile float *Setpoint,
		float Kp,
		float Ki,
		float Kd,
		PIDCD_TypeDef ControllerDirection
		)
{
    uPID->MyOutput   = Output;
    uPID->MyInput    = Input;
    uPID->MySetpoint = Setpoint;
    uPID->InAuto     = (PIDMode_TypeDef)0;        // 初始为手动模式

    PID_SetOutputLimits(uPID, 0, DEFAULT_PWM_MAX);
    uPID->SampleTime = DEFAULT_SAMPLE_TIME_MS;

    PID_SetControllerDirection(uPID, ControllerDirection);
    PID_SetTunings(uPID, Kp, Ki, Kd);

    uPID->LastTime = SysTick_GetTick() - uPID->SampleTime;  // 确保第一次调用立即计算
}

/**
 * @brief 浮点数 clamp 工具函数
 */
float float_clamp(float d, float min, float max) {
      const float t = d < min ? min : d;
      return t > max ? max : t;
}

/**
 * @brief 检查浮点数是否在 [min, max] 范围外
 * @return 1 = 超出范围（会触发clamp）, 0 = 在范围内
 */
uint8_t check_clamping(float d, float min, float max) {
      if(d > max || d < min){
          return 1;
      }
      else{
          return 0;
      }
}

/**
 * @brief 执行一次PID计算
 * @return 1 = 已计算, 0 = 未计算（时间未到）
 *
 * 计算流程：
 * 1. 检查是否在自动模式
 * 2. 检查是否已达到采样时间（或设为每次调用都计算）
 * 3. 计算时间差 timeChange_in_seconds
 * 4. P项 = Kp * error
 * 5. D项 = -Kd * dInput / dt （测量值微分）
 * 6. I项 = Ki * error * dt （条件积分 + 非对称增益）
 * 7. 输出 = P + I + D，clamp到 [OutMin, OutMax]
 */
uint8_t PID_Compute(PID_TypeDef *uPID){
    uint32_t now;
    uint32_t timeChange;
    float timeChange_in_seconds;

    float input;
    float error;
    float dInput;
    float output;

    /* 仅在自动模式下计算 */
    if (!uPID->InAuto){
        return 0;
    }

    /* 计算距离上次执行的时间差 */
    now        = SysTick_GetTick();
    timeChange = (now - uPID->LastTime);

    if ((timeChange >= uPID->SampleTime) || (uPID->updateOnEveryCall))
    {
        /* 防止零除：如果在同一tick内多次调用 */
        if (timeChange == 0){
            return 0;
        }
        timeChange_in_seconds = timeChange/1000.0f;

        /* 计算误差相关变量 */
        input   = *uPID->MyInput;
        error   = *uPID->MySetpoint - input;       // 误差 = 设定值 - 测量值
        dInput  = (input - uPID->LastInput);        // 输入变化量（用于微分项）

        /* ---- P项：比例于误差 ---- */
        uPID->DispKp_part = uPID->Kp * error;
        output = uPID->DispKp_part;

        /* ---- D项：测量值微分（避免设定点突变的微分冲击） ---- */
        uPID->DispKd_part = - (uPID->Kd / timeChange_in_seconds) * dInput;
        output += uPID->DispKd_part;

        /* ---- I项：条件积分（抗饱和 + 非对称增益） ---- */
        /* 条件1：如果输出已饱和且增大积分只会更饱和，暂停积分 */
        if(check_clamping(output + uPID->NegativeErrorIgainMultiplier*uPID->Ki * error  * timeChange_in_seconds, uPID->OutMin, uPID->OutMax) && (error*(output + uPID->OutputSum) > 0)){
            uPID->OutputSum     += 0;               // 抗饱和：跳过本次积分
        }
        /* 条件2：正常误差范围，使用标准积分增益 */
        else if(error > -1){                         // -1容差，允许在零误差附近保持输出
            uPID->OutputSum     += (uPID->Ki * error * timeChange_in_seconds);
        }
        /* 条件3：负误差（过冲），使用降低的积分增益 */
        else{
            uPID->OutputSum     += uPID->NegativeErrorIgainMultiplier*(uPID->Ki * error * timeChange_in_seconds);
        }

        /* I项限幅（防积分饱和） */
        uPID->OutputSum = float_clamp(uPID->OutputSum, uPID->IMin, uPID->IMax);

        /* 如果设定值设为0（睡眠/紧急停止），清除积分累加器 */
        if(*uPID->MySetpoint == 0){
            uPID->OutputSum = 0;
        }

        /* 积分分离：误差超过IminError阈值时，清除积分累加器 */
        if(error > fabsf(uPID->IminError)){
            uPID->OutputSum = 0;
        }

        uPID->DispKi_part = uPID->OutputSum;

        /* 最终输出 = P + I + D */
        output += uPID->DispKi_part;

        /* 输出限幅 */
        output = float_clamp(output, uPID->OutMin, uPID->OutMax);

        *uPID->MyOutput = output;

        /* 保存状态供下次计算使用 */
        uPID->LastInput = input;
        uPID->LastTime = now;

        return 1;  // 计算已完成
    }
    else{
        return 0;  // 时间未到，跳过
    }
}

/**
 * @brief 设置PID模式（自动/手动切换）
 * 从手动切到自动时，调用PID_Init复位内部状态（防冲击）
 */
void PID_SetMode(PID_TypeDef *uPID, PIDMode_TypeDef Mode){
    uint8_t newAuto = (Mode == PID_MODE_AUTOMATIC);

    if (newAuto && !uPID->InAuto){
        PID_Init(uPID);  // 重新初始化：设置LastInput=当前输入，clamp OutputSum
    }
    uPID->InAuto = (PIDMode_TypeDef)newAuto;
}

PIDMode_TypeDef PID_GetMode(PID_TypeDef *uPID){
    return uPID->InAuto ? PID_MODE_AUTOMATIC : PID_MODE_MANUAL;
}

/**
 * @brief 设置PID输出限幅
 * 如果当前输出和积分累加器超出新范围，会被clamp。
 */
void PID_SetOutputLimits(PID_TypeDef *uPID, float Min, float Max){
    if (Min >= Max) return;

    uPID->OutMin = Min;
    uPID->OutMax = Max;

    if (uPID->InAuto){
        *uPID->MyOutput = float_clamp(*uPID->MyOutput, uPID->OutMin, uPID->OutMax);
        uPID->OutputSum = float_clamp(uPID->OutputSum, uPID->OutMin, uPID->OutMax);
    }
}

/**
 * @brief 设置积分累加器限幅（防止积分饱和）
 */
void PID_SetILimits(PID_TypeDef *uPID, float Min, float Max){
    if (Min >= Max) return;
    uPID->IMin = Min;
    uPID->IMax = Max;
}

/**
 * @brief 设置积分分离的最小误差阈值
 * 当 |error| > IminError 时，积分累加器清零
 */
void PID_SetIminError(PID_TypeDef *uPID, float IminError){
    if (IminError < 0) return;
    uPID->IminError = IminError;
}

/**
 * @brief 设置负误差时的I增益系数（非对称积分）
 *
 * 在温度过冲时（实际温度 > 设定温度），误差为负，
 * 通过降低I增益可以加速系统从过冲中恢复。
 *
 * @param NegativeErrorIgainMultiplier 负误差I增益乘数（通常 < 1.0）
 * @param NegativeErrorIgainBias       偏移量（保留参数）
 */
void PID_SetNegativeErrorIgainMult(PID_TypeDef *uPID, float NegativeErrorIgainMultiplier, float NegativeErrorIgainBias){
    if (NegativeErrorIgainMultiplier < 0) return;
    uPID->NegativeErrorIgainMultiplier = NegativeErrorIgainMultiplier;
    uPID->NegativeErrorIgainBias = NegativeErrorIgainBias;
}

/**
 * @brief 设置PID增益参数
 * 当控制方向为REVERSE时，内部实际存储的增益值为负值。
 */
void PID_SetTunings(PID_TypeDef *uPID, float Kp, float Ki, float Kd){
    if (Kp < 0 || Ki < 0 || Kd < 0) return;

    uPID->DispKp = Kp;   // 用户设置值（始终为正）
    uPID->DispKi = Ki;
    uPID->DispKd = Kd;

    uPID->Kp = Kp;       // 内部实际值
    uPID->Ki = Ki;
    uPID->Kd = Kd;

    /* 反向控制：取负增益 */
    if (uPID->ControllerDirection == PID_CD_REVERSE){
        uPID->Kp = (0 - uPID->Kp);
        uPID->Ki = (0 - uPID->Ki);
        uPID->Kd = (0 - uPID->Kd);
    }
}

/**
 * @brief 设置PID控制方向
 * 切换方向时，如果已在自动模式，需要反转增益值。
 */
void PID_SetControllerDirection(PID_TypeDef *uPID, PIDCD_TypeDef Direction){
    if ((uPID->InAuto) && (Direction !=uPID->ControllerDirection)){
        uPID->Kp = (0 - uPID->Kp);
        uPID->Ki = (0 - uPID->Ki);
        uPID->Kd = (0 - uPID->Kd);
    }
    uPID->ControllerDirection = Direction;
}

PIDCD_TypeDef PID_GetDirection(PID_TypeDef *uPID){
    return uPID->ControllerDirection;
}

/**
 * @brief 设置PID采样时间
 * @param NewSampleTime 采样间隔（毫秒）
 * @param updateOnCall  非0时每次调用PID_Compute都计算（忽略采样时间）
 *
 * 注：Ki和Kd的计算使用实际时间差（timeChange_in_seconds），
 *     因此更改采样时间无需调整增益值。
 */
void PID_SetSampleTime(PID_TypeDef *uPID, int32_t NewSampleTime, int32_t updateOnCall){
    if(updateOnCall > 0){
        updateOnCall = 1;
    }
    uPID->updateOnEveryCall = updateOnCall;

    if (NewSampleTime > 0){
        uPID->SampleTime = (uint32_t)NewSampleTime;
    }
}

/* ---- 获取PID参数 ---- */
float PID_GetKp(PID_TypeDef *uPID){ return uPID->DispKp; }
float PID_GetKi(PID_TypeDef *uPID){ return uPID->DispKi; }
float PID_GetKd(PID_TypeDef *uPID){ return uPID->DispKd; }

/* ---- 获取当前PID各项贡献值 ---- */
float PID_GetPpart(PID_TypeDef *uPID){ return uPID->DispKp_part; }  // P项贡献
float PID_GetIpart(PID_TypeDef *uPID){ return uPID->DispKi_part; }  // I项贡献
float PID_GetDpart(PID_TypeDef *uPID){ return uPID->DispKd_part; }  // D项贡献
