/**
 * @file    hysteresis.h
 * @brief   迟滞滤波器头文件 - 抑制传感器读数的微小波动
 *
 * 该滤波器只在数值变化超过设定的迟滞量时才更新输出，
 * 从而消除传感器噪声引起的数值抖动。
 */
#ifndef INC_HYSTERESIS_H
#define INC_HYSTERESIS_H

/* 包含文件 ------------------------------------------------------------------*/
#include "stdint.h"
#include <math.h>

/* 类型定义 ------------------------------------------------------------------*/

/**
 * @brief 迟滞滤波器结构体
 */
typedef struct{
    float previous_value;   // 上次输出的滤波值
    float hysteresis;       // 迟滞量（变化必须超过此值才更新输出）
}Hysteresis_FilterTypeDef;

/* 函数原型 -------------------------------------------------------*/

/**
 * @brief 初始化迟滞滤波器
 * @param hysteresis_struct 滤波器结构体指针
 * @param hysteresis        迟滞量
 */
void hysteresis_init(Hysteresis_FilterTypeDef* hysteresis_struct, float hysteresis);

/**
 * @brief 迟滞滤波处理
 * @param new_value         新的采样值
 * @param hysteresis_struct 滤波器结构体指针
 * @return 滤波后的值（仅当变化超过迟滞量时才输出新值）
 */
float hysteresis_add(float new_value, Hysteresis_FilterTypeDef* hysteresis_struct);

#endif
