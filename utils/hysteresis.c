#include "hysteresis.h"

/**
 * @brief 初始化迟滞滤波器
 * @param hysteresis_struct 滤波器结构体
 * @param hysteresis        迟滞量（变化必须超过此值才触发更新）
 */
void hysteresis_init(Hysteresis_FilterTypeDef* hysteresis_struct, float hysteresis)
{
    hysteresis_struct->hysteresis = hysteresis;
    hysteresis_struct->previous_value = NAN;  // 用NaN标记未初始化状态
}

/**
 * @brief 迟滞滤波处理
 * @param new_value         新采样值
 * @param hysteresis_struct 滤波器结构体
 * @return 滤波后的输出值
 *
 * 逻辑：
 * 1. 如果尚未初始化（previous_value为NaN），直接接受新值
 * 2. 如果新值超出 [previous_value - hysteresis, previous_value + hysteresis] 范围，更新输出
 * 3. 否则保持上次输出不变
 */
float hysteresis_add(float new_value, Hysteresis_FilterTypeDef* hysteresis_struct) {
    if (isnan(hysteresis_struct->previous_value)) {
        hysteresis_struct->previous_value = new_value;
        return new_value;
    }

    if ((new_value >= hysteresis_struct->previous_value + hysteresis_struct->hysteresis) ||
        (new_value <= hysteresis_struct->previous_value - hysteresis_struct->hysteresis)) {
        hysteresis_struct->previous_value = new_value;
    }

    return hysteresis_struct->previous_value;
}
