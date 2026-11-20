// Copyright 2025 Lukas Hrazky
// Modified by Refloat rink-mixed (refloatRM), 2026. See FORK_NOTES.md.
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

#include "alert_tracker.h"
#include "atr.h"
#include "battery_curve.h"
#include "bms.h"
#include "booster.h"
#include "brake_tilt.h"
#include "charging.h"
#include "data_record.h"
#include "footpad_sensor.h"
#include "haptic_feedback.h"
#include "imu.h"
#include "konami.h"
#include "lcm.h"
#include "lcm_fw.h"
#include "leds.h"
#include "motor_control.h"
#include "motor_data.h"
#include "pid.h"
#include "remote.h"
#include "state.h"
#include "time.h"
#include "torque_tilt.h"
#include "turn_tilt.h"

#include <vesc_c_if.h>

// Re-RinkFloat: horn settings + custom melody in custom EEPROM words
#define HORN_EEPROM_WORDS 34  // 2 settings words + 32 melody words (64 notes/rests)

typedef struct {
    lib_thread main_thread;
    lib_thread aux_thread;

    RefloatConfig float_conf;

    // Firmware version, passed in from Lisp
    int fw_version_major, fw_version_minor, fw_version_beta;

    // IMU data for the balancing filter
    BalanceFilterData balance_filter;

    Time time;
    MotorData motor;
    IMU imu;
    PID pid;
    MotorControl motor_control;

    TorqueTilt torque_tilt;
    ATR atr;
    BrakeTilt brake_tilt;
    TurnTilt turn_tilt;
    Booster booster;
    Remote remote;

    State state;
    FootpadSensor footpad;
    HapticFeedback haptic_feedback;
    AlertTracker alert_tracker;

    Leds leds;
    LcmData lcm;
    LcmFw lcm_fw;  // LCM firmware update relay
    BatteryCurve battery_curve;  // battery % for the LCM status bar
    volatile bool battery_save_pending;
    volatile time_t battery_change_time;
    uint32_t battery_saved[4];
    Charging charging;
    BMS bms;

    DataRecord data_record;

    Konami flywheel_konami;
    Konami headlights_on_konami;
    Konami headlights_off_konami;

    // Beeper
    int beep_num_left;
    float beep_duration;
    time_t beeper_timer;
    int beep_reason;
    bool beeper_enabled;
    bool sw_horn_beeping;  // Re-RinkFloat: external beeper is on because of the horn

    // Re-RinkFloat: horn settings + boot melody switch, remembered on the board
    // (2 custom EEPROM words after the config). Written from the aux thread,
    // only when changed and never while riding.
    volatile uint32_t horn_saved[HORN_EEPROM_WORDS];
    volatile uint32_t horn_pending[HORN_EEPROM_WORDS];
    volatile bool horn_save_pending;
    volatile time_t horn_change_time;
    uint8_t horn_save_retries;
    // light module (ALCM VP) settings save state (see lcm_vp_save_if_due)
    volatile bool lcm_vp_save_pending;
    volatile time_t lcm_vp_change_time;
    uint32_t lcm_vp_saved[LCM_VP_EEPROM_WORDS];  // failed saves retried at most 3 times
    bool boot_melody;
    // rf77 safety: Refloat config changes made while not at rest (e.g. the
    // app mirroring a light switch mid-ride) are applied in RAM at once and
    // written to flash once the board is at rest
    volatile bool cfg_save_pending;
    // rf77 safety: a flash write is in progress - starting is refused
    volatile bool flash_busy;
    time_t start_blocked_beep_timer;
    bool boot_melody_custom;  // play the custom melody at boot
    float boot_melody_volume;
    uint8_t melody_custom[128];  // 64 x (pitch level 0 = rest, length in 20 ms), 0 length = end
    uint32_t horn_last_w0, horn_last_w1;  // last horn settings words from the app

    // Config values
    uint32_t loop_time_us;
    float startup_pitch_trickmargin, startup_pitch_tolerance;
    float startup_step_size;
    float tiltback_duty_step_size, tiltback_hv_step_size, tiltback_lv_step_size,
        tiltback_return_step_size;
    float tiltback_variable, tiltback_variable_max_erpm, noseangling_step_size;
    bool duty_beeping;

    float balance_current;

    float setpoint, setpoint_target, setpoint_target_interpolated;
    float noseangling_interpolated;
    time_t alert_timer;
    time_t nag_timer;
    float idle_voltage;
    time_t fault_angle_pitch_timer, fault_angle_roll_timer, fault_switch_timer,
        fault_switch_half_timer;
    time_t wheelslip_timer, tb_highvoltage_timer;
    float switch_warn_beep_erpm;
    bool traction_control;

    // Darkride aka upside down mode:
    bool is_upside_down_started;  // dark ride has been engaged
    bool enable_upside_down;  // dark ride mode is enabled (10 seconds after fault)
    time_t upside_down_fault_timer;

    // Feature: Flywheel
    bool flywheel_abort;

    // Feature: Reverse Stop
    float reverse_stop_step_size, reverse_tolerance, reverse_total_erpm;
    time_t reverse_timer;

    // Feature: Soft Start
    float softstart_pid_limit, softstart_ramp_step_size;

    uint64_t odometer;

    // Feature: RC Move (control via app while idle)
    int rc_steps;
    int rc_counter;
    float rc_current_target;
    float rc_current;
} Data;
