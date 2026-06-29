/**
 * @file    state.c
 * @brief   Soldering station state machine (ported from AxxSolder)
 *
 * State transitions:
 *   SLEEP --(pick up handle/encoder)--> RUN
 *   RUN --(handle in stand)--> PRESTANDBY --(delay N sec)--> STANDBY
 *   STANDBY --(timeout N min)--> SLEEP
 *   STANDBY/PRESTANDBY --(pick up handle)--> RUN
 *   RUN --(continuous heat timeout)--> EMERGENCY_SLEEP
 *   Any --(overtemp/undervolt/fault)--> EMERGENCY_SLEEP
 *   Any heating --(short press encoder)--> HALTED
 *   HALTED --(short press encoder)--> RUN
 */

#include "state.h"
#include "heater.h"
#include "buzzer.h"
#include "config.h"

float PID_setpoint = 0.0f;
/* Internal state tracking */
static uint32_t s_prestandby_start_ms = 0;
static uint32_t s_standby_start_ms    = 0;
static uint32_t s_run_start_ms        = 0;
static uint8_t  s_was_in_stand        = 0;
static uint8_t  s_tip_detected        = 0;

/* ========================================================================
 * state_init — initialize state machine to SLEEP
 * ======================================================================== */
void state_init(void)
{
    sensor_val.current_state  = STATE_SLEEP;
    sensor_val.previous_state = STATE_SLEEP;
    PID_setpoint = 0.0f;
    s_was_in_stand   = 0;
    s_tip_detected   = 0;
    s_run_start_ms   = 0;
}

/* ========================================================================
 * change_state — transition to new state with entry actions
 * ======================================================================== */
void change_state(m_state_t new_state)
{
    if (sensor_val.current_state == new_state) return;

    sensor_val.previous_state = sensor_val.current_state;
    sensor_val.current_state  = new_state;

    switch (new_state) {

    case STATE_RUN:
        PID_setpoint = sensor_val.temp_target;
        s_run_start_ms = millis();
        break;

    case STATE_PRESTANDBY:
        s_prestandby_start_ms = millis();
        break;

    case STATE_STANDBY:
        s_standby_start_ms = millis();
        break;

    case STATE_SLEEP:
        PID_setpoint = 0.0f;
        heater_off();
        break;

    case STATE_EMERGENCY_SLEEP:
        PID_setpoint = 0.0f;
        heater_off();
        buzzer_trigger(BUZZ_DOUBLE);
        break;

    case STATE_HALTED:
        PID_setpoint = 0.0f;
        heater_off();
        break;

    default:
        break;
    }
}

/* ========================================================================
 * state_update — main state machine logic (called every 10ms)
 *
 * Handles stand detection, standby/sleep timeouts, encoder wake-up.
 * ======================================================================== */
void state_update(void)
{
    float in_stand = sensor_val.sleep;
    m_state_t current  = sensor_val.current_state;

    /* Tip presence detection via temperature change */
    float tc_now  = sensor_val.temp_avg;
    float tc_prev = sensor_val.temp_last;
    float tc_delta = tc_now - tc_prev;
    if (tc_delta < 0.0f) tc_delta = -tc_delta;

    if (tc_delta > 5.0f) {
        s_tip_detected = 1;
    } else if (sensor_val.current < 0.05f && tc_now < 35.0f) {
        s_tip_detected = 0;
    }

    /* State transitions */
    switch (current) {

    case STATE_RUN:
        PID_setpoint = sensor_val.temp_target;

        /* Handle in stand -> pre-standby or immediate standby */
        if (in_stand > 0.5f && !s_was_in_stand) {
            if (config_val.standby_delay_s > 0.5f) {
                change_state(STATE_PRESTANDBY);
            } else {
                change_state(STATE_STANDBY);
            }
        }

        /* Continuous heating timeout protection */
        if (config_val.emergency_time > 0.5f) {
            uint32_t elapsed = millis() - s_run_start_ms;
            if (elapsed >= (uint32_t)(config_val.emergency_time * 60000.0f)) {
                change_state(STATE_EMERGENCY_SLEEP);
            }
        }
        break;

    case STATE_PRESTANDBY:
        /* Handle picked up -> back to RUN */
        if (in_stand < 0.5f && s_was_in_stand) {
            change_state(STATE_RUN);
            break;
        }
        /* Delay elapsed -> enter standby */
        if (millis() - s_prestandby_start_ms >=
            (uint32_t)(config_val.standby_delay_s * 1000.0f)) {
            change_state(STATE_STANDBY);
        }
        break;

    case STATE_STANDBY:
        /* Handle picked up -> back to RUN */
        if (in_stand < 0.5f) {
            change_state(STATE_RUN);
            break;
        }
        /* Setpoint = min(user target, standby temp) */
        if (config_val.standby_temp > sensor_val.temp_target) {
            PID_setpoint = sensor_val.temp_target;
        } else {
            PID_setpoint = config_val.standby_temp;
        }
        /* Sleep timeout -> full sleep */
        if (config_val.sleep_timeout_min > 0.5f) {
            if (millis() - s_standby_start_ms >=
                (uint32_t)(config_val.sleep_timeout_min * 60000.0f)) {
                change_state(STATE_SLEEP);
            }
        }
        break;

    case STATE_SLEEP:
        PID_setpoint = 0.0f;
        heater_off();
        /* Wake on handle pickup + tip detected */
        if (in_stand < 0.5f && s_tip_detected) {
            change_state(STATE_RUN);
        }
        break;

    case STATE_EMERGENCY_SLEEP:
        /* Only recoverable via power cycle */
        PID_setpoint = 0.0f;
        heater_off();
        break;

    case STATE_HALTED:
        /* Heater off, resume via encoder short press */
        PID_setpoint = 0.0f;
        heater_off();
        break;

    default:
        break;
    }

    s_was_in_stand = (in_stand > 0.5f) ? 1 : 0;
}

/* ========================================================================
 * state_emergency_check — fault condition check (called every 10ms)
 *
 * Checks (after 1s startup stabilization delay):
 *   1. Overtemp (>490C)
 *   2. Undervoltage (<8V, RUN state only)
 *   3. TC fault (heating but no temp rise)
 * ======================================================================== */
void state_emergency_check(void)
{
    /* Startup stabilization: wait 100 ticks (1 second) before checking */
    static uint16_t s_startup_ticks = 0;
    if (s_startup_ticks < 100) {
        s_startup_ticks++;
        return;
    }

    /* Overtemp */
    if (sensor_val.temp_avg > EMERGENCY_SHUTDOWN_TEMP) {
        change_state(STATE_EMERGENCY_SLEEP);
        return;
    }

    /* Undervoltage */
    if (sensor_val.voltage < MIN_BUS_VOLTAGE &&
        sensor_val.current_state == STATE_RUN) {
        change_state(STATE_EMERGENCY_SLEEP);
        return;
    }

    /* TC fault: heating but temp < 50C and setpoint > 100C
     * Possible causes: no tip, broken TC wire, amp failure */
    if (sensor_val.current_state == STATE_RUN &&
        sensor_val.current > 0.5f &&
        sensor_val.temp_avg < 50.0f &&
        PID_setpoint > 100.0f) {
        /* TODO: add debounce counter */
    }
}

/* ========================================================================
 * state_name — human-readable state name
 * ======================================================================== */
const char *state_name(m_state_t state)
{
    switch (state) {
    case STATE_RUN:             return "RUN";
    case STATE_PRESTANDBY:      return "PRE-STBY";
    case STATE_STANDBY:         return "STANDBY";
    case STATE_SLEEP:           return "SLEEP";
    case STATE_EMERGENCY_SLEEP: return "FAULT!";
    case STATE_HALTED:          return "HALTED";
    default:                    return "UNKNOWN";
    }
}
