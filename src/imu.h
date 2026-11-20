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

#include "balance_filter.h"
#include "state.h"
#include "time.h"

typedef struct {
    float pitch;
    float balance_pitch;
    float roll;
    float yaw;
    float pitch_rate;

    float flywheel_pitch_offset;
    float flywheel_roll_offset;

    // rf77: IMU data timeout (ported from upstream v1.3). data_timer is
    // refreshed by the firmware's IMU read callback (imu_data_received).
    volatile time_t data_timer;  // written by the IMU thread, read by the main loop
    time_t timeout_timer;
    bool timeout;
} IMU;

void imu_init(IMU *imu, const Time *time);

// Called from the firmware's IMU read callback: fresh data arrived.
void imu_data_received(IMU *imu, const Time *time);

/**
 * Checks for an IMU data timeout, storing the state on the IMU object. To be
 * called periodically from the main loop. Returns true while timed out.
 */
bool imu_check_timeout(IMU *imu, const Time *time);

/**
 * True when the IMU data is flowing and has been for long enough for the
 * filter to reconverge from its stale pre-timeout state.
 */
bool imu_data_settled(const IMU *imu, const Time *time);

void imu_update(IMU *imu, const BalanceFilterData *bf, const State *state);

void imu_set_flywheel_offsets(IMU *imu);
