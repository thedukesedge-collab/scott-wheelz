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
#include "footpad_sensor.h"
#include "led_driver.h"
#include "led_strip.h"
#include "state.h"

#define LEDS_REFRESH_RATE 30

typedef struct {
    LedTransition transition;
    float split;
} TransitionState;

typedef struct {
    bool enabled;
    bool headlights_enabled;
} LedsRuntimeStatus;

typedef struct {
    LedStrip status_strip;
    LedStrip front_strip;
    LedStrip rear_strip;

    const CfgLeds *cfg;

    float last_updated;
    State state;
    float pitch;

    float left_sensor;
    float right_sensor;

    float on_off_fade;

    float duty_threshold;
    float status_duty_blend;
    float status_idle_blend;
    float status_idle_time;
    float status_animation_start;

    float status_on_front_blend;
    float status_on_front_idle_blend;
    float status_on_front_idle_time;
    bool board_is_upright;

    LedsRuntimeStatus runtime_status;
    // represents the state of a given flag being overriden at runtime
    LedsRuntimeStatus runtime_status_overriden;

    bool headlights_on;
    bool direction_forward;
    float split_distance;
    float headlights_time;
    float animation_start;

    float confirm_animation_start;

    TransitionState headlights_trans;
    TransitionState dir_trans;

    const LedBar *front_bar;
    const LedBar *front_dir_target;
    const LedBar *front_time_target;

    const LedBar *rear_bar;
    const LedBar *rear_dir_target;
    const LedBar *rear_time_target;

    uint32_t *led_data;
    LedDriver led_driver;

    // rf80: STATUS BAR OFF from the app (LEDS tab) for Refloat's own LEDs
    // (Refloat has no status-only switch): the status strip is dimmed to 0
    bool status_disabled;

    // rf86: snooze for Refloat's own LEDs (set each aux pass from the stored
    // light settings): after snooze_timeout s without movement, footpad or
    // engage, all strips dim to a slow breath (or dark with snooze_dark)
    float snooze_timeout;  // 0 = never
    bool snooze_dark;
    bool snooze_dim;  // rf97: steady low glow instead of the breath
    // rf97: riding status bar: 0 = Refloat's battery / duty bar, else the
    // Status Idle colors / speed with Refloat animation ride_mode - 1 (the duty
    // bar still takes over above the duty threshold)
    uint8_t ride_mode;

    // rf88: power-off warning. VESC powers off after shutdown_timeout s
    // (App Settings > Shutdown Mode, 0 = no timer) without motor commands;
    // motor_active_time is when the package last drove / braked the motor
    // (what resets VESC's timer). In the last minute the lights blink twice
    // every 15 s.
    float shutdown_timeout;
    float motor_active_time;
} Leds;

void leds_init(Leds *leds);

void leds_setup(Leds *leds, CfgHwLeds *hw_cfg, const CfgLeds *cfg);

void leds_configure(Leds *leds, const CfgLeds *cfg);

const LedsRuntimeStatus *leds_get_runtime_status(const Leds *leds);

void leds_set_enabled(Leds *leds, bool value);

void leds_set_headlights_enabled(Leds *leds, bool value);

void leds_update(Leds *leds, const State *state, FootpadSensorState fs_state);

void leds_status_confirm(Leds *leds);

void leds_destroy(Leds *leds);
