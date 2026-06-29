/**
 * @file    average.c
 * @brief   滑动平均滤波器实现
 *
 * 使用循环缓冲区实现高效 O(1) 滑动平均：
 * - sum 维护窗口内所有元素的总和
 * - 每次新数据到达时：sum = sum + 新值 - 被替换的最旧值
 * - 平均值 = sum / length
 */

#include "average.h"

void average_init(t_average_def *filter, uint8_t length) {
  if (length == 0)
    length = 1;
  if (length > MAX_WINDOW_LENGTH)
    length = MAX_WINDOW_LENGTH;
  filter->length = length;
  filter->sum = 0;
  filter->pointer = 0;
  for (uint8_t i = 0; i < filter->length; i++) {
    filter->history[i] = 0;
  }
}

float average_compute(float raw_data, t_average_def *filter) {
  /* 更新累加器：加新值、减最旧值 */
  filter->sum += raw_data;
  filter->sum -= filter->history[filter->pointer];
  filter->history[filter->pointer] = raw_data;

  /* 循环移动窗口指针 */
  if (filter->pointer < filter->length - 1) {
    filter->pointer += 1;
  } else {
    filter->pointer = 0;
  }

  return (float)filter->sum / (float)filter->length;
}

void average_set(float raw_data, t_average_def *filter) {
  /* 用指定值快速填充整个窗口，避免滤波器收敛延迟 */
  filter->sum = raw_data * filter->length;
  filter->pointer = 0;
  for (uint8_t i = 0; i < filter->length; i++) {
    filter->history[i] = raw_data;
  }
}
