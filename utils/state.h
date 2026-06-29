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
void change_state(SOLDER_STATE_Def new_state);
const char *state_name(SOLDER_STATE_Def state);

extern float PID_setpoint;

#endif /* SOURCE_STATEMACHINE_H */
