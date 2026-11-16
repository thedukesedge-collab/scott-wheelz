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

#pragma once

#include "conf/datatypes.h"
#include "motor_control.h"
#include "motor_data.h"
#include "state.h"
#include "time.h"

typedef enum {
    HAPTIC_FEEDBACK_NONE = 0,
    HAPTIC_FEEDBACK_DUTY_SPEED,
    HAPTIC_FEEDBACK_DUTY_CONTINUOUS,
    HAPTIC_FEEDBACK_ERROR_TEMPERATURE,
    HAPTIC_FEEDBACK_ERROR_VOLTAGE,
    HAPTIC_FEEDBACK_ERROR_FATAL,
    HAPTIC_FEEDBACK_HORN,  // Scott Wheelz: manual horn from the app
} HapticFeedbackType;

// Scott Wheelz horn sound modes (sent by the app with each press)
typedef enum {
    HORN_MODE_BEEP = 0,  // audible motor tone only
    HORN_MODE_BUZZ = 1,  // vibration (current) only; tone when idle or vibrate is off
    HORN_MODE_ALT = 2,  // alternates beep and buzz every HORN_ALT_STEP
    HORN_MODE_BOTH = 3,  // beep and buzz together
} HornMode;

typedef struct {
    const CfgHapticFeedback *cfg;
    float duty_solid_threshold;
    float str_poly_b;
    float str_poly_c;

    HapticFeedbackType type_playing;
    time_t tone_timer;
    bool is_playing;
    bool can_change_type;

    // Scott Wheelz horn: set from the command thread, read in the main loop.
    // A press plays for at least HORN_MIN_TIME and at most HORN_MAX_TIME,
    // stops HORN_TIMEOUT after the last keepalive (e.g. app disconnects), and
    // a new press is ignored until HORN_COOLDOWN after the previous release.
    volatile time_t horn_timer;  // last request from the app
    volatile time_t horn_start;  // start of the current press
    volatile time_t horn_end;  // release of the previous press
    volatile bool horn_on;  // button currently held
    volatile bool horn_used;  // at least one press since boot
    volatile uint8_t horn_mode;  // HornMode of the current press
} HapticFeedback;

void haptic_feedback_init(HapticFeedback *hf);

void haptic_feedback_configure(HapticFeedback *hf, const RefloatConfig *cfg);

// Scott Wheelz: request (on=true) or cancel (on=false) the haptic horn.
void haptic_feedback_horn(HapticFeedback *hf, const Time *time, bool on, uint8_t mode);

// Scott Wheelz: true while the horn is actually sounding (drives the optional
// external beeper on the PPM pin).
bool haptic_feedback_horn_playing(const HapticFeedback *hf);

void haptic_feedback_update(
    HapticFeedback *hf,
    MotorControl *mc,
    const State *state,
    const MotorData *md,
    const AlertTracker *at,
    const Time *time
);
