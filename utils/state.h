/**
 * @file    statemachine.h
 * @brief   Soldering station state machine interface
 */

#ifndef SOURCE_STATEMACHINE_H
#define SOURCE_STATEMACHINE_H

#include "config.h"


void state_init(void);
void state_update(void);
void state_emergency_check(void);
void change_state(m_state_t new_state);
const char *state_name(m_state_t state);

extern float PID_setpoint;

#endif /* SOURCE_STATEMACHINE_H */
