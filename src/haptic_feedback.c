// Copyright 2024 Lukas Hrazky
//
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

#include "haptic_feedback.h"

#include "conf/datatypes.h"
#include "utils.h"
#include "vesc_c_if.h"

#include <math.h>

#define TONE_LENGTH 0.1f
// Rink-Float horn safety limits (seconds)
// - stops this long after the last keepalive from the app (app disconnects)
#define HORN_TIMEOUT 0.3f
// - a tap plays at least this long, so it's noticeable
#define HORN_MIN_TIME 0.2f
// - one press never plays longer than this; holding longer is silent
#define HORN_MAX_TIME 1.0f
// - after a release, new presses are ignored for this long. Together with
//   HORN_MAX_TIME this caps the horn at ~50% duty, which matters most when
//   idle: the tone then runs at standstill with no airflow cooling.
#define HORN_COOLDOWN 1.0f
// - in ALT mode, beep and buzz swap this often
#define HORN_ALT_STEP 0.125f

void haptic_feedback_init(HapticFeedback *hf) {
    hf->type_playing = HAPTIC_FEEDBACK_NONE;
    hf->tone_timer = 0;
    hf->is_playing = false;
    hf->can_change_type = true;
    hf->horn_timer = 0;
    hf->horn_start = 0;
    hf->horn_end = 0;
    hf->horn_on = false;
    hf->horn_used = false;
    hf->horn_mode = HORN_MODE_BOTH;
}

void haptic_feedback_horn(HapticFeedback *hf, const Time *time, bool on, uint8_t mode) {
    // A press whose keepalives stopped arriving counts as released now.
    if (hf->horn_on && timer_older(time, hf->horn_timer, HORN_TIMEOUT)) {
        hf->horn_on = false;
        hf->horn_end = time->now;
    }

    if (!on) {
        if (hf->horn_on) {
            hf->horn_on = false;
            hf->horn_end = time->now;
        }
        return;
    }

    if (hf->horn_on) {
        // keepalive for the current press (does not extend HORN_MAX_TIME)
        hf->horn_timer = time->now;
        return;
    }

    // new press: ignored during the cooldown after the previous one
    if (hf->horn_used && !timer_older(time, hf->horn_end, HORN_COOLDOWN)) {
        return;
    }

    hf->horn_mode = mode <= HORN_MODE_BOTH ? mode : HORN_MODE_BOTH;
    hf->horn_used = true;
    hf->horn_start = time->now;
    hf->horn_timer = time->now;
    hf->horn_on = true;
}

static inline bool horn_active(const HapticFeedback *hf, const Time *time) {
    if (!hf->horn_used || timer_older(time, hf->horn_start, HORN_MAX_TIME)) {
        return false;
    }

    // minimum length for short taps
    if (!timer_older(time, hf->horn_start, HORN_MIN_TIME)) {
        return true;
    }

    return hf->horn_on && !timer_older(time, hf->horn_timer, HORN_TIMEOUT);
}

void haptic_feedback_configure(HapticFeedback *hf, const RefloatConfig *cfg) {
    hf->cfg = &cfg->haptic;
    hf->duty_solid_threshold = cfg->tiltback_duty + hf->cfg->duty_solid_offset;

    // pre-calculate the coefficients of the polynomial given the configured max strength speed
    float m = hf->cfg->max_strength_speed > 0 ? hf->cfg->max_strength_speed : 1;
    float a = hf->cfg->min_strength;
    hf->str_poly_b = (1 - hf->cfg->strength_curvature) * (1 - a) / m;
    hf->str_poly_c = (1 - a - hf->str_poly_b * m) / (m * m);
}

static HapticFeedbackType haptic_feedback_get_type(
    const HapticFeedback *hf,
    const State *state,
    const MotorData *md,
    const AlertTracker *at,
    const Time *time
) {
    // Rink-Float: the horn also works while the board is idle (READY).
    // update() then plays only the foc_play_tone part, which runs the motor
    // at 0 A with an audio signal on top, so the wheel isn't pushed.
    if (state->state == STATE_READY && horn_active(hf, time)) {
        return HAPTIC_FEEDBACK_HORN;
    }

    // TODO: Ideally we don't even do pushback in handtest, as it can be confusing
    if (state->state != STATE_RUNNING || state->mode == MODE_HANDTEST) {
        return HAPTIC_FEEDBACK_NONE;
    }

    if (at->fatal_error) {
        return HAPTIC_FEEDBACK_ERROR_FATAL;
    }

    switch (state->sat) {
    case SAT_PB_DUTY:
        if (md->duty_cycle > hf->duty_solid_threshold) {
            return HAPTIC_FEEDBACK_DUTY_CONTINUOUS;
        } else {
            return HAPTIC_FEEDBACK_DUTY_SPEED;
        }
    case SAT_PB_SPEED:
        return HAPTIC_FEEDBACK_DUTY_SPEED;
    case SAT_PB_TEMPERATURE:
        return HAPTIC_FEEDBACK_ERROR_TEMPERATURE;
    case SAT_PB_LOW_VOLTAGE:
    case SAT_PB_HIGH_VOLTAGE:
    case SAT_PB_ERROR:
        return HAPTIC_FEEDBACK_ERROR_VOLTAGE;
    default:
        break;
    }

    if (hf->cfg->current_threshold > 0.0f &&
        motor_data_get_current_saturation(md) > hf->cfg->current_threshold) {
        return HAPTIC_FEEDBACK_DUTY_CONTINUOUS;
    }

    // Rink-Float: the horn has the lowest priority, so real pushback and
    // error alerts are never masked by it.
    if (horn_active(hf, time)) {
        return HAPTIC_FEEDBACK_HORN;
    }

    return HAPTIC_FEEDBACK_NONE;
}

// Returns the number of "beats" per period of a given tone. Tones are played
// on even beats and if there are more than two beats, the last beat is
// skipped, giving a certain number of "beeps" followed by a pause.
static uint8_t get_beats(HapticFeedbackType type) {
    switch (type) {
    case HAPTIC_FEEDBACK_DUTY_SPEED:
        return 2;
    case HAPTIC_FEEDBACK_DUTY_CONTINUOUS:
    case HAPTIC_FEEDBACK_HORN:
        return 0;
    case HAPTIC_FEEDBACK_ERROR_TEMPERATURE:
        return 6;
    case HAPTIC_FEEDBACK_ERROR_VOLTAGE:
        return 8;
    case HAPTIC_FEEDBACK_ERROR_FATAL:
        return 10;
    case HAPTIC_FEEDBACK_NONE:
        break;
    }

    return 0;
}

static const CfgHapticTone *get_haptic_tone(const HapticFeedback *hf) {
    switch (hf->type_playing) {
    case HAPTIC_FEEDBACK_DUTY_SPEED:
    case HAPTIC_FEEDBACK_DUTY_CONTINUOUS:
    case HAPTIC_FEEDBACK_HORN:
        return &hf->cfg->duty;
    case HAPTIC_FEEDBACK_ERROR_TEMPERATURE:
    case HAPTIC_FEEDBACK_ERROR_VOLTAGE:
    case HAPTIC_FEEDBACK_ERROR_FATAL:
        return &hf->cfg->error;
    case HAPTIC_FEEDBACK_NONE:
        break;
    }

    return 0;
}

static inline float strength_scale(const HapticFeedback *hf, float speed) {
    return min(hf->cfg->min_strength + hf->str_poly_b * speed + hf->str_poly_c * speed * speed, 1);
}

static inline void foc_play_tone(int channel, float freq, float voltage) {
    if (!VESC_IF->foc_play_tone) {
        return;
    }

    VESC_IF->foc_play_tone(channel, freq, voltage);
}

void haptic_feedback_update(
    HapticFeedback *hf,
    MotorControl *mc,
    const State *state,
    const MotorData *md,
    const AlertTracker *at,
    const Time *time
) {
    HapticFeedbackType type_to_play = haptic_feedback_get_type(hf, state, md, at, time);

    if (type_to_play != hf->type_playing && hf->can_change_type) {
        hf->type_playing = type_to_play;
        timer_refresh(time, &hf->tone_timer);
    }

    bool should_be_playing = false;
    if (hf->type_playing != HAPTIC_FEEDBACK_NONE) {
        uint8_t beats = get_beats(hf->type_playing);
        if (beats == 0) {
            should_be_playing = true;
            hf->can_change_type = true;
        } else {
            float period = TONE_LENGTH * beats;
            float tone_time = fmodf(timer_age(time, hf->tone_timer), period);
            uint8_t beat = floorf(tone_time / TONE_LENGTH);
            uint8_t off_beat = beats > 2 ? beats - 2 : 0;

            should_be_playing = beat % 2 == 0 && (off_beat == 0 || beat != off_beat);
            // Only allow changing type (another pattern or stop alerting)
            // if we just finished a period
            hf->can_change_type = !hf->is_playing && beat == 0;
        }
    } else {
        hf->can_change_type = true;
    }

    if (hf->is_playing && !should_be_playing) {
        foc_play_tone(0, 1, 0.0f);
        motor_control_stop_tone(mc);
        hf->is_playing = false;
    } else if (should_be_playing) {
        const CfgHapticTone *tone = get_haptic_tone(hf);
        // Rink-Float: the horn always plays at the configured maximum
        // strength (the strength reached at Max Strength Speed), regardless of
        // the current speed.
        float scale =
            hf->type_playing == HAPTIC_FEEDBACK_HORN ? 1.0f : strength_scale(hf, fabsf(md->speed));

        bool play_beep = tone->strength > 0.0f;
        bool play_buzz = hf->cfg->vibrate.strength > 0.0f;

        if (hf->type_playing == HAPTIC_FEEDBACK_HORN) {
            // Rink-Float: no current-based vibration when the board isn't
            // running, so an idle horn never moves the wheel.
            bool can_buzz = play_buzz && state->state == STATE_RUNNING;
            switch (hf->horn_mode) {
            case HORN_MODE_BEEP:
                play_buzz = false;
                break;
            case HORN_MODE_BUZZ:
                // fall back to the beep if buzzing isn't possible
                play_buzz = can_buzz;
                play_beep = play_beep && !can_buzz;
                break;
            case HORN_MODE_ALT: {
                uint32_t step = (uint32_t) (timer_age(time, hf->horn_start) / HORN_ALT_STEP);
                bool beep_step = (step % 2) == 0;
                // idle / no vibrate: beep, pause, beep, ...
                play_buzz = can_buzz && !beep_step;
                play_beep = play_beep && beep_step;
                break;
            }
            default:
                play_buzz = can_buzz;
                break;
            }
        }

        bool horn = hf->type_playing == HAPTIC_FEEDBACK_HORN;

        if (play_beep) {
            foc_play_tone(0, tone->frequency, tone->strength * scale);
        } else if (horn) {
            foc_play_tone(0, 1, 0.0f);
        }

        if (play_buzz) {
            motor_control_play_tone(
                mc, hf->cfg->vibrate.frequency, hf->cfg->vibrate.strength * scale
            );
        } else if (horn) {
            motor_control_stop_tone(mc);
        }

        hf->is_playing = true;
    }
}

bool haptic_feedback_horn_playing(const HapticFeedback *hf) {
    return hf->type_playing == HAPTIC_FEEDBACK_HORN && hf->is_playing;
}
