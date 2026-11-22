// Copyright 2024 Lukas Hrazky
// Modified by Refloat Glow, 2026. See FORK_NOTES.md.
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
    HAPTIC_FEEDBACK_HORN,  // Refloat Glow: manual horn from the app
    // Refloat Glow (hapticspeedsplit): a speed reminder tone with its own
    // threshold, separate from Speed Pushback. Lowest priority, beep only.
    HAPTIC_FEEDBACK_SPEED_TONE,
} HapticFeedbackType;

// Refloat Glow (hapticspeedsplit): speed reminder limits (km/h). 0 = off;
// below the minimum it stays off (no reminder buzzing at walking speed).
#define SPEED_TONE_MIN_KMH 16  // 10 mph
#define SPEED_TONE_MAX_KMH 100
// one short beep per period: clearly different from the duty / speed
// pushback tone (fast beeping) and the error patterns
#define SPEED_TONE_PERIOD 1.5f

// Refloat Glow horn sound modes (sent by the app with each press)
typedef enum {
    HORN_MODE_BEEP = 0,  // audible motor tone only
    HORN_MODE_BUZZ = 1,  // vibration (current) only; tone when idle or vibrate is off
    HORN_MODE_ALT = 2,  // alternates beep and buzz every HORN_ALT_STEP
    HORN_MODE_BOTH = 3,  // beep and buzz together
} HornMode;

// Refloat Glow: melodies are up to 8 notes and 4 s long
#define MELODY_MAX_NOTES 64  // notes + rests
#define MELODY_MAX_LENGTH 4.0f  // whole melody incl. rests (s), = recording window
#define MELODY_MAX_NOTE 2.0f  // one note / one held tile (s); rests are not capped
#define MELODY_UNIT 0.02f  // note length unit (s)
// rf133: pitch level 11 = a short beep on the controller's own beeper (Refloat's,
// Servo/PPM) instead of a motor tone; silent when the beeper is off
#define MELODY_BEEP_LEVEL 11
#define MELODY_BEEP_HZ 1
// Refloat Glow: boot melody volume (beep V), kept well under the
// horn's 4 V on purpose; the app's 1-10 maps to 0.25-2.5 V
#define BOOT_MELODY_MAX_V 2.5f
#define BOOT_MELODY_DEFAULT_V 1.5f
// Refloat Glow: sound budget shared by melodies and live tiles, so the motor
// never plays tones for long stretches: 10 s of sound, refilled at half
// speed while quiet; when it runs out, tones pause until 3 s is back.
#define TONE_BUDGET 10.0f
#define TONE_BUDGET_REFILL 0.5f
#define TONE_BUDGET_RESUME 3.0f

// a note is 2 bytes: pitch level 0-10 (0 = rest), length in 20 ms units
uint16_t melody_level_hz(uint8_t level);

typedef struct {
    const CfgHapticFeedback *cfg;
    float duty_solid_threshold;
    float str_poly_b;
    float str_poly_c;

    HapticFeedbackType type_playing;
    time_t tone_timer;
    bool is_playing;
    bool can_change_type;

    // Refloat Glow horn: set from the command thread, read in the main loop.
    // A press plays for at least HORN_MIN_TIME and at most HORN_MAX_TIME_*
    // (riding / idle), stops HORN_TIMEOUT after the last keepalive (e.g. app
    // disconnects), and is limited by a heat budget (see haptic_feedback.c).
    volatile time_t horn_timer;  // last request from the app
    volatile time_t horn_start;  // start of the current press
    volatile float horn_budget;  // seconds of horn available (main loop)
    time_t horn_budget_timer;
    volatile bool horn_on;  // button currently held
    volatile bool horn_used;  // at least one press since boot
    volatile uint8_t horn_mode;  // HornMode of the current press

    // Refloat Glow: horn-only sound settings sent by the app (not saved on the
    // board). Until they arrive, the horn uses the haptic config with minimums.
    volatile bool horn_custom;
    volatile float horn_beep_strength;  // V
    volatile uint16_t horn_beep_frequency;  // Hz
    volatile float horn_buzz_strength;  // A, 0 = no buzz
    volatile uint16_t horn_buzz_frequency;  // Hz

    // Refloat Glow: melody player (boot melody / custom melody), READY only
    volatile bool melody_active;
    volatile time_t melody_start;
    volatile float melody_volume;  // V
    uint16_t melody_freq[MELODY_MAX_NOTES];  // Hz, 0 = rest, MELODY_BEEP_HZ = controller beep
    float melody_len[MELODY_MAX_NOTES];  // s
    volatile uint8_t melody_count;
    uint8_t melody_beep_step;  // rf133: last step that asked for a controller beep (+1)
    bool beep_request;  // rf133: main sounds one short controller beep, then clears it
    volatile uint16_t live_hz;
    volatile time_t live_timer;
    volatile time_t live_start;
    volatile float live_volume;
    bool live_playing;
    volatile uint8_t speed_tone_kmh;  // hapticspeedsplit: 0 = off
    float tone_budget;  // s of tone left
    time_t tone_budget_timer;
    bool tone_rest;  // budget ran out, waiting for TONE_BUDGET_RESUME
} HapticFeedback;

void haptic_feedback_init(HapticFeedback *hf);

// Refloat Glow: horn-only settings from the app; values are clamped to safe ranges.
void haptic_feedback_horn_settings(
    HapticFeedback *hf,
    float beep_strength,
    uint16_t beep_frequency,
    float buzz_strength,
    uint16_t buzz_frequency
);

void haptic_feedback_configure(HapticFeedback *hf, const RefloatConfig *cfg);

// Refloat Glow (hapticspeedsplit): set the speed reminder (km/h, 0 = off).
// Values under SPEED_TONE_MIN_KMH turn it off, above the max are capped.
uint8_t haptic_feedback_set_speed_tone(HapticFeedback *hf, uint8_t kmh);

// Refloat Glow horn requests (byte [2] of COMMAND_SW_HORN)
typedef enum {
    HORN_REQ_OFF = 0,
    HORN_REQ_KEEPALIVE = 1,  // only keeps an existing press alive
    HORN_REQ_START = 2,  // starts a new press
} HornRequest;

// Refloat Glow: start / keep alive / stop the haptic horn.
void haptic_feedback_horn(HapticFeedback *hf, const Time *time, uint8_t req, uint8_t mode);

// Refloat Glow: true while the horn is actually sounding (drives the optional
// external beeper on the PPM pin).
bool haptic_feedback_horn_playing(const HapticFeedback *hf);

// Refloat Glow: plays a melody of `count` notes (2 bytes each, see above) at
// the given beep volume; count 0 plays the default boot melody (the rider's
// "Melody 2", see haptic_feedback.c). Only sounds while the board is READY; stops if it engages.
void haptic_feedback_play_melody(
    HapticFeedback *hf, const Time *time, float volume, const uint8_t *notes, uint8_t count
);

// Refloat Glow: live tone for the melody tiles: plays pitch `level` (0 = stop)
// while keepalives arrive (stops 0.3 s after the last one). READY only.
void haptic_feedback_live_tone(HapticFeedback *hf, const Time *time, uint8_t level, float volume);

void haptic_feedback_update(
    HapticFeedback *hf,
    MotorControl *mc,
    const State *state,
    const MotorData *md,
    const AlertTracker *at,
    const Time *time
);
