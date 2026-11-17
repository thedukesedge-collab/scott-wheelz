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

#include "footpad_sensor.h"
#include "leds.h"
#include "motor_data.h"
#include "state.h"

#include <stddef.h>

typedef enum {
    COMMAND_LCM_POLL = 24,  // this should only be called by external light modules
    COMMAND_LCM_LIGHT_INFO = 25,  // to be called by apps to get lighting info
    COMMAND_LCM_LIGHT_CTRL = 26,  // to be called by apps to change light settings
    COMMAND_LCM_DEVICE_INFO = 27,  // to be called by apps to check lighting controller firmware
    COMMAND_LCM_GET_BATTERY = 29,

    COMMAND_LCM_DEBUG = 99,  // reserved for external debug purposes
} LcmCommands;

#define MAX_LCM_NAME_LENGTH 20
#define MAX_LCM_PAYLOAD_LENGTH 64

// Re-Float Rink-Mixed: light settings for the VP build of ALCM, set from the
// app (COMMAND_LCM_VP_SETTINGS) and appended to every LCM poll reply. See
// lcm/README.md for the wire format.
#define LCM_VP_FLAG_HEADLIGHTS 0x01
#define LCM_VP_FLAG_STATUS_BAR 0x02
#define LCM_VP_FLAG_BEEP 0x04
#define LCM_VP_FLAG_TEST_BEEP 0x08  // toggled on each test-beep request (not stored)
#define LCM_VP_FLAG_UPDATE 0x10  // firmware update staged: start the LCM bootloader (not stored)
#define LCM_VP_FLAG_DOZE_DARK 0x20  // headlights off in snooze (no breathing)
#define LCM_VP_STORED_FLAGS                                                                        \
    (LCM_VP_FLAG_HEADLIGHTS | LCM_VP_FLAG_STATUS_BAR | LCM_VP_FLAG_BEEP | LCM_VP_FLAG_DOZE_DARK)
#define LCM_VP_ANIMATIONS 5  // idle, riding, dozing (snooze), boot, shutdown
#define LCM_VP_ANIMATION_COUNT 14  // 0 = standard/off .. 12, 13 = battery gauge (SLCM r16+)
#define LCM_VP_SETTINGS_LENGTH 12  // bytes in COMMAND_LCM_VP_SETTINGS (11 accepted)
#define LCM_VP_EEPROM_WORDS 4

typedef struct {
    bool valid;  // set from the app at least once (otherwise nothing is sent)
    uint8_t flags;
    uint8_t headlight_brightness;  // % while riding
    uint8_t headlight_idle_brightness;  // % when not riding
    uint8_t status_idle_brightness;  // %
    uint8_t status_riding_brightness;  // %
    uint8_t animations[LCM_VP_ANIMATIONS];
    uint16_t color;  // degrees 0-359
} LcmVpSettings;

typedef struct {
    bool enabled;
    uint8_t brightness;
    uint8_t brightness_idle;
    uint8_t status_brightness;
    bool lights_off_when_lifted;

    char name[MAX_LCM_NAME_LENGTH];
    uint8_t payload[MAX_LCM_PAYLOAD_LENGTH];
    uint8_t payload_size;

    LcmVpSettings vp;
    bool vp_test_beep;  // toggled by COMMAND_LCM_TEST_BEEP, sent as LCM_VP_FLAG_TEST_BEEP
    bool vp_update;  // set from lcm_fw_update_flag() before each poll reply
    uint8_t vp_battery;  // battery % from the curve (battery_curve.h), 0xFF = VESC's own
} LcmData;

void lcm_init(LcmData *lcm, CfgHwLeds *hw_cfg);

void lcm_configure(LcmData *lcm, const Leds *leds);

/**
 * Poll request from LCM with any data that need to be passed to the package.
 */
void lcm_poll_request(LcmData *lcm, uint8_t *buffer, size_t len);

// Re-Float Rink-Mixed: VP settings set from the app and the LCM named itself
// as SLCM ("VP-..." / "VPBL-...") or doesn't name itself (older SLCM):
// only then is the VP block sent
bool lcm_is_slcm(const LcmData *lcm);

/**
 * Response to the LCM poll request to get data from the package.
 */
void lcm_poll_response(
    LcmData *lcm,
    const State *state,
    FootpadSensorState fs_state,
    const MotorData *motor,
    const float pitch
);

/**
 * Command for apps to call to get info about lighting.
 */
void lcm_light_info_response(const LcmData *lcm);

/**
 * Command for apps to call to get LCM hardware info (if any).
 */
void lcm_device_info_response(const LcmData *lcm);

/**
 * No idea why this exists. Seems it should have been part of lcm_poll_response().
 */
void lcm_get_battery_response(const LcmData *lcm);

/**
 * Command for apps to call to control LCM details (lights, behavior, etc).
 */
void lcm_light_ctrl_request(LcmData *lcm, unsigned char *cfg, int len);

/**
 * Re-Float Rink-Mixed: sets the VP light settings from the app (12 bytes:
 * flags, headlight riding %, status idle %, status riding %, 5 animations,
 * color hi, color lo, headlight idle %; the 11-byte form without the last
 * byte uses the riding % for idle too). Values are clamped. Returns true if
 * anything changed.
 */
bool lcm_vp_set(LcmData *lcm, const uint8_t *buffer, int len);

/**
 * Packs / unpacks the VP light settings to / from LCM_VP_EEPROM_WORDS EEPROM
 * words (word 3 = headlight idle %; 0 / missing = same as riding).
 */
void lcm_vp_pack(const LcmData *lcm, uint32_t words[LCM_VP_EEPROM_WORDS]);
void lcm_vp_unpack(LcmData *lcm, const uint32_t words[LCM_VP_EEPROM_WORDS]);

/**
 * Asks the light module for a test beep (VP firmware; needs VP settings).
 */
void lcm_vp_test_beep(LcmData *lcm);
