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

#include "speed_limit.h"

#include "lib/utils.h"

#include <math.h>

// Bounds for the pushback angle (Pushback Angle, tiltback_duty_angle, is
// clamped to them). The maximum is the default Low Voltage Pushback angle,
// the steepest pushback Refloat applies by default.
#define ANGLE_MIN 3.0f
#define ANGLE_MAX 10.0f
// Additional pushback angle per km/h above the limit.
#define ANGLE_PER_KMH 1.0f

// Bounds for the rate of the pushback (Pushback Speed, tiltback_duty_speed)
// and of its release (Return To Level Speed, tiltback_return_speed), in °/s.
// The minimum is the default rate of the Voltage Pushbacks and of the return,
// a zero rate would disable the pushback (or never release it). The maximum
// avoids abrupt setpoint changes at speed.
#define RATE_MIN 1.0f
#define RATE_MAX 5.0f

void speed_limit_init(SpeedLimit *sl) {
    speed_limit_reset(sl);
}

void speed_limit_reset(SpeedLimit *sl) {
    sl->active = false;
    sl->direction = 1;
    sl->angle = 0.0f;
}

uint8_t speed_limit_threshold(uint8_t configured) {
    if (configured == 0 || configured > SPEED_LIMIT_MAX) {
        return SPEED_LIMIT_MAX;
    }

    return max(configured, SPEED_LIMIT_MIN);
}

void speed_limit_sanitize_config(RefloatConfig *config) {
    config->tiltback_speed = speed_limit_threshold(config->tiltback_speed);
    // Remote move drives the board without a rider, keep it under the limit too
    config->remote.max_move_speed = min(config->remote.max_move_speed, SPEED_LIMIT_MAX);
}

void speed_limit_update(
    SpeedLimit *sl,
    const MotorData *motor,
    const State *state,
    const RefloatConfig *config,
    float dt
) {
    // Flywheel isn't a riding mode, the board stands on its nose
    if (state->mode == MODE_FLYWHEEL) {
        speed_limit_reset(sl);
        return;
    }

    float release_rate = clampf(config->tiltback_return_speed, RATE_MIN, RATE_MAX);

    // During wheelslip the wheel speed isn't the speed of the board, hold the
    // pushback as it is until traction is regained. At the duty cycle limit,
    // release it, like the other pushbacks are (see calculate_setpoint_target).
    if (state->wheelslip) {
        sl->active = false;
        if (motor->duty_cycle.value > motor->duty_max_with_margin) {
            rate_limitf(&sl->angle, 0.0f, release_rate * dt);
        }
        return;
    }

    // mc_get_speed() can be overridden from LispBM (firmware 6.06+), so also
    // calculate the speed from ERPM and use the higher of the two
    float speed = fmaxf(fabsf(motor->speed), motor->abs_erpm * motor->erpm_to_speed);
    float threshold = speed_limit_threshold(config->tiltback_speed);
    sl->active = speed > threshold;

    // Only change direction once the pushback is fully released, so that it
    // never flips sides abruptly
    if (sl->angle == 0.0f) {
        sl->direction = motor->erpm_sign;
    }

    float target = 0.0f;
    float rate = release_rate;
    if (motor->erpm_sign != sl->direction && motor->abs_erpm > ERPM_MOVING_THRESHOLD) {
        // Moving the other way: release quickly, so that the pushback can
        // apply in the new direction of travel and doesn't override the other
        // pushbacks in it
        rate = RATE_MAX;
    } else if (sl->active) {
        float base_angle = clampf(config->tiltback_duty_angle, ANGLE_MIN, ANGLE_MAX);
        target = fminf(base_angle + (speed - threshold) * ANGLE_PER_KMH, ANGLE_MAX);
        if (target > sl->angle) {
            rate = clampf(config->tiltback_duty_speed, RATE_MIN, RATE_MAX);
        }
    }

    rate_limitf(&sl->angle, target, rate * dt);
}

// Returns how much (0 to 1) an offset lowering the nose is neutralized. It
// rises with the pushback angle to keep the setpoint continuous.
static float neutralization(const SpeedLimit *sl) {
    return fminf(sl->angle / ANGLE_MIN, 1.0f);
}

float speed_limit_apply_pushback(const SpeedLimit *sl, float setpoint) {
    float value = setpoint * sl->direction;
    if (value >= 0.0f) {
        value = fmaxf(value, sl->angle);
    } else {
        value = value * (1.0f - neutralization(sl)) + sl->angle;
    }

    return value * sl->direction;
}

float speed_limit_apply_offset(const SpeedLimit *sl, float offset) {
    float value = offset * sl->direction;
    if (value < 0.0f) {
        value *= 1.0f - neutralization(sl);
    }

    return value * sl->direction;
}
