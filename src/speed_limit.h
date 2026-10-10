// This file is part of the Refloat VESC package.
//
// Refloat VESC package is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by the
// Free Software Foundation, either version 3 of the License, or (at your
// option) any later version.
//
// Refloat VESC package is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
// or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
// more details.
//
// You should have received a copy of the GNU General Public License along with
// this program. If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include "conf/datatypes.h"
#include "motor_data.h"
#include "state.h"

#include <stdbool.h>
#include <stdint.h>

// Speed Limit: a mandatory speed pushback for EDPM compliance, see doc/edpm.md.
//
// It can't be disabled and its threshold can't be configured above
// SPEED_LIMIT_MAX, by any means (VESC Tool, apps, runtime commands). It never
// cuts or limits the motor power, that would make the board nosedive. It
// raises the nose of the board, progressively with the overspeed, forcing the
// rider into an uncomfortable stance to make them slow down.

// Maximum design speed of an EDPM (French Code de la route, article R311-1,
// 6.15) in km/h. The hard ceiling of the speed limit.
#define SPEED_LIMIT_MAX 25
// The lowest configurable speed limit in km/h.
#define SPEED_LIMIT_MIN 10

typedef struct {
    bool active;  // speed is above the limit
    int8_t direction;  // direction of travel the pushback applies to
    float angle;  // rate-limited pushback angle (magnitude)
} SpeedLimit;

void speed_limit_init(SpeedLimit *sl);

void speed_limit_reset(SpeedLimit *sl);

/**
 * Returns the effective speed limit for a configured value. 0 (which used to
 * mean "disabled") and values above SPEED_LIMIT_MAX result in SPEED_LIMIT_MAX,
 * values below SPEED_LIMIT_MIN result in SPEED_LIMIT_MIN.
 */
uint8_t speed_limit_threshold(uint8_t configured);

/**
 * Makes the configuration conform to the speed limit, so that it shows the
 * values actually in effect. The Speed Limit itself doesn't rely on this (its
 * effective threshold is derived again when used), the Remote move speed does.
 */
void speed_limit_sanitize_config(RefloatConfig *config);

void speed_limit_update(
    SpeedLimit *sl,
    const MotorData *motor,
    const State *state,
    const RefloatConfig *config,
    float dt
);

/**
 * Applies the pushback to the base setpoint (other pushbacks and setpoint
 * adjustments). In the direction of travel, the higher of the two is used, so
 * pushbacks don't add up.
 */
float speed_limit_apply_pushback(const SpeedLimit *sl, float setpoint);

/**
 * Applies the pushback to a setpoint offset controlled by the rider or the
 * configuration (Remote, Nose Angling). An offset lowering the nose is
 * neutralized as the pushback rises, so that it can't cancel it.
 *
 * Not meant for the Tilts (ATR, Torque Tilt, Brake Tilt, Turn Tilt): they
 * compensate for the slope and the load, neutralizing ATR on a downhill would
 * raise the nose relative to the ground.
 */
float speed_limit_apply_offset(const SpeedLimit *sl, float offset);
