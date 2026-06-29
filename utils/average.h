/**
 * @file    moving_average.h
 * @brief   Moving average filter header (ported from AxxSolder)
 */

#ifndef SOURCE_MOVING_AVERAGE_H
#define SOURCE_MOVING_AVERAGE_H

#include <stdint.h>

#define MAX_WINDOW_LENGTH 50

typedef struct {
  uint8_t length;
  float history[MAX_WINDOW_LENGTH];
  float sum;
  uint8_t pointer;
} t_average_def;

void average_init(t_average_def *filter, uint8_t length);
float average_compute(float raw_data, t_average_def *filter);
void average_set(float raw_data, t_average_def *filter);

#endif /* SOURCE_MOVING_AVERAGE_H */
