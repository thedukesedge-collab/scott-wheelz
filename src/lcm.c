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

#include "lcm.h"

#include "vesc_c_if.h"

#include "conf/buffer.h"
#include "utils.h"

#include <math.h>
#include <string.h>

static uint8_t refloat_idle_anim(const LcmData *lcm, uint8_t custom[LCM_VP_CUSTOM_LENGTH]);

void lcm_init(LcmData *lcm, CfgHwLeds *hw_cfg) {
    lcm->enabled = hw_cfg->mode & LED_MODE_EXTERNAL;
    lcm->brightness = 0;
    lcm->brightness_idle = 0;
    lcm->status_brightness = 0;
    lcm->name[0] = '\0';
    lcm->payload_size = 0;
    lcm->lights_off_when_lifted = true;
    lcm->vp.valid = false;
    lcm->vp_test_beep = false;
    lcm->vp_update = false;
    lcm->vp_battery = 0xFF;
}

void lcm_configure(LcmData *lcm, const Leds *leds) {
    lcm->leds_cfg = leds->cfg;  // whatever the LED Mode (LCM_VP_ANIM_REFLOAT)
    if (!lcm->enabled) {
        return;
    }

    const CfgLeds *cfg = leds->cfg;
    const LedsRuntimeStatus *status = leds_get_runtime_status(leds);

    if (!status->enabled) {
        lcm->brightness = 0.0f;
        lcm->brightness_idle = 0.0f;
        lcm->status_brightness = 0.0f;
    } else {
        if (status->headlights_enabled) {
            lcm->brightness = cfg->headlights.brightness * 100;
            lcm->status_brightness = cfg->status.brightness_headlights_on * 100;
        } else {
            // Re-RinkFloat: headlights off really means off on a light module
            // (ALCM etc. have one headlight brightness; the stock "front strip
            // brightness" default equals the headlight default, so toggling
            // changed nothing)
            lcm->brightness = 0;
            lcm->status_brightness = cfg->status.brightness_headlights_off * 100;
        }
        // Re-Float Rink-Mixed: and no idle (running) light either
        lcm->brightness_idle = status->headlights_enabled ? cfg->front.brightness * 100 : 0;
    }
    lcm->lights_off_when_lifted = cfg->lights_off_when_lifted;
}

int lcm_slcm_rev(const LcmData *lcm) {
    const char *n = lcm->name;
    if (n[0] != 'V' || n[1] != 'P') {
        return 0;
    }
    n += n[2] == 'B' ? 5 : 3;  // "VPBL-" / "VP-"
    if (*n++ != 'r') {
        return 0;
    }
    int rev = 0;
    while (*n >= '0' && *n <= '9' && rev < 10000) {
        rev = rev * 10 + (*n++ - '0');
    }
    return rev;
}

bool lcm_is_slcm(const LcmData *lcm) {
    // rf70: also an LCM that doesn't name itself - SLCM / ALCM VP builds from
    // before r15 (rf67-rf69 cut them off, and their LED settings with them).
    // Stock ALCM ignores the extra bytes, as before rf67. Only an LCM that
    // names itself as something else gets the plain stock reply.
    return lcm->vp.valid &&
        (lcm->name[0] == '\0' ||
         (lcm->name[0] == 'V' && lcm->name[1] == 'P' && (lcm->name[2] == '-' || lcm->name[2] == 'B')
         ));
}

void lcm_poll_request(LcmData *lcm, uint8_t *buffer, size_t len) {
    // Re-Float Rink-Mixed: the name is kept whatever the LED mode (the app
    // uses it to tell which LCM firmware is fitted: "VPBL-..." = ALCM VP
    // with the bootloader), and `i >= len` (stock read one byte past it)

    // Optionally pass in LCM name and version in a single string
    if (len > 0) {
        for (size_t i = 0; i < MAX_LCM_NAME_LENGTH; i++) {
            if (i >= len || i > MAX_LCM_NAME_LENGTH - 1 || buffer[i] == '\0') {
                lcm->name[i] = '\0';
                break;
            }
            lcm->name[i] = buffer[i];
        }
    }
}

void lcm_poll_response(
    LcmData *lcm,
    const State *state,
    FootpadSensorState fs_state,
    const MotorData *motor,
    const float pitch
) {
    static const int bufsize = 20 + MAX_LCM_PAYLOAD_LENGTH;
    uint8_t buffer[bufsize];
    int32_t ind = 0;

    buffer[ind++] = 101;  // Package ID
    buffer[ind++] = COMMAND_LCM_POLL;

    // Re-Float Rink-Mixed: the VP light settings (and the VP block) only go to
    // an SLCM - an LCM that named itself "VP-..." / "VPBL-..." in its poll -
    // whatever the LED Mode. Any other LCM gets exactly stock Refloat's reply.
    const bool slcm = lcm_is_slcm(lcm);
    if (lcm->enabled || slcm) {
        uint8_t send_state = state_compat(state) & 0xF;
        send_state += fs_state << 4;
        if (state->mode == MODE_HANDTEST) {
            send_state |= 0x80;
        }

        buffer[ind++] = send_state;
        buffer[ind++] = VESC_IF->mc_get_fault();

        if (state->state == STATE_RUNNING) {
            buffer[ind++] = fminf(100, fabsf(motor->duty_cycle * 100));
        } else {
            // pitch is a value between -180 and +180, so abs(pitch) fits into uint8
            buffer[ind++] = lcm->lights_off_when_lifted ? fabsf(pitch) : 0;
        }

        buffer_append_float16(buffer, motor->erpm, 1e0, &ind);
        buffer_append_float16(buffer, VESC_IF->mc_get_tot_current_in(), 1e0, &ind);
        buffer_append_float16(buffer, VESC_IF->mc_get_input_voltage_filtered(), 1e1, &ind);

        if (slcm) {
            // Re-Float Rink-Mixed: brightness from the app (status bar level
            // depends on riding / not riding), then the VP block (ALCM VP
            // build); 25 bytes in total, ALCM accepts up to 32
            // Headlights and status bar both switch between their riding
            // and idle levels here, so the LCM just follows the first one.
            const LcmVpSettings *vp = &lcm->vp;
            const bool riding = state->state == STATE_RUNNING;
            // headlights off: 0 too, so "off" is off even on an LCM that only
            // follows the brightness (the SLCM also switches off on the flag)
            const bool head_on = vp->flags & LCM_VP_FLAG_HEADLIGHTS;
            buffer[ind++] = !head_on ? 0
                : riding             ? vp->headlight_brightness
                                     : vp->headlight_idle_brightness;
            buffer[ind++] = head_on ? vp->headlight_idle_brightness : 0;
            buffer[ind++] = riding ? vp->status_riding_brightness : vp->status_idle_brightness;
            buffer[ind++] = 0x56;  // 'V'
            buffer[ind++] = 1;  // version
            // rf77: bit 6 (power off, SLCM r22) is never set - the package
            // can't power the board off
            buffer[ind++] = vp->flags | (lcm->vp_test_beep ? LCM_VP_FLAG_TEST_BEEP : 0) |
                (lcm->vp_update ? LCM_VP_FLAG_UPDATE : 0);
            // idle following Refloat: its animation, translated (the
            // fill / scan ones need SLCM r19+, older builds get STANDARD)
            uint8_t custom[LCM_VP_CUSTOM_LENGTH];
            memcpy(custom, vp->custom, sizeof(custom));
            const int rev = lcm_slcm_rev(lcm);
            for (int i = 0; i < LCM_VP_ANIMATIONS; ++i) {
                uint8_t anim = vp->animations[i];
                if (anim == LCM_VP_ANIM_REFLOAT) {
                    // riding: the rider's own CUSTOM animation stays in the
                    // block (the SLCM has one), the idle one isn't playing
                    uint8_t idle_custom[LCM_VP_CUSTOM_LENGTH];
                    anim = refloat_idle_anim(lcm, riding ? idle_custom : custom);
                    if (anim == 14 && rev < LCM_SLCM_CUSTOM_REV) {
                        anim = 0;
                    }
                }
                buffer[ind++] = anim;
            }
            buffer[ind++] = vp->color >> 8;
            buffer[ind++] = vp->color & 0xFF;
            buffer[ind++] = lcm->vp_battery;  // 0xFF = use the VESC's battery %
            // CUSTOM animation: only to an SLCM that takes it (older builds
            // drop replies over 32 bytes)
            if (rev >= LCM_SLCM_CUSTOM_REV) {
                for (int i = 0; i < LCM_VP_CUSTOM_LENGTH; ++i) {
                    buffer[ind++] = custom[i];
                }
            }
            lcm->payload_size = 0;
        } else {
            // LCM control info
            buffer[ind++] = lcm->brightness;
            buffer[ind++] = lcm->brightness_idle;
            buffer[ind++] = lcm->status_brightness;

            // Relay any generic byte pairs set by cmd_light_ctrl
            for (uint8_t i = 0; i < lcm->payload_size; ++i) {
                buffer[ind++] = lcm->payload[i];
            }
            lcm->payload_size = 0;  // Message has been processed, clear it
        }
    }

    SEND_APP_DATA(buffer, bufsize, ind);
}

void lcm_light_info_response(const LcmData *lcm) {
    static const int bufsize = 15;
    uint8_t buffer[bufsize];
    int32_t ind = 0;

    buffer[ind++] = 101;  // Package ID
    buffer[ind++] = COMMAND_LCM_LIGHT_INFO;

    // Lights control for Refloat is not compatible with this interface; Send 3
    // for LCM (3 is he identifier for external led module in Float), otherwise 0.
    buffer[ind++] = lcm->enabled ? 3 : 0;

    if (lcm->enabled) {
        buffer[ind++] = lcm->brightness;
        buffer[ind++] = lcm->brightness_idle;
        buffer[ind++] = lcm->status_brightness;

        // Don't send Float-specific configuration.
        buffer[ind++] = 0;  // led_mode
        buffer[ind++] = 0;  // mode_idle
        buffer[ind++] = 0;  // status_mode
        buffer[ind++] = 0;  // status_count
        buffer[ind++] = 0;  // forward_count
        buffer[ind++] = 0;  // rear_count
    }

    SEND_APP_DATA(buffer, bufsize, ind);
}

void lcm_device_info_response(const LcmData *lcm) {
    static const int bufsize = MAX_LCM_NAME_LENGTH + 2;
    uint8_t buffer[bufsize];
    int32_t ind = 0;

    buffer[ind++] = 101;  // Package ID
    buffer[ind++] = COMMAND_LCM_DEVICE_INFO;

    {
        // Write LCM firmware name into the buffer (Re-Float Rink-Mixed:
        // whatever the LED mode, "" when no LCM has named itself)
        for (int i = 0; i < MAX_LCM_NAME_LENGTH; i++) {
            buffer[ind++] = lcm->name[i];
            if (lcm->name[i] == '\0') {
                break;
            }
        }
    }

    SEND_APP_DATA(buffer, bufsize, ind);
}

void lcm_get_battery_response(const LcmData *lcm) {
    static const int bufsize = 10;
    uint8_t buffer[bufsize];
    int32_t ind = 0;

    buffer[ind++] = 101;  // Package ID
    buffer[ind++] = COMMAND_LCM_GET_BATTERY;

    if (lcm->enabled) {
        buffer_append_float32_auto(buffer, VESC_IF->mc_get_battery_level(NULL), &ind);
    }

    SEND_APP_DATA(buffer, bufsize, ind);
}

void lcm_light_ctrl_request(LcmData *lcm, unsigned char *cfg, int len) {
    if (!lcm->enabled || len < 3) {
        return;
    }

    int32_t idx = 0;

    // rf77: clamped as in upstream v1.3 (a bad packet must not overflow
    // payload[] - payload_size sits right after it)
    lcm->brightness = min(cfg[idx++], 100u);
    lcm->brightness_idle = min(cfg[idx++], 100u);
    lcm->status_brightness = min(cfg[idx++], 100u);

    if (len > 3) {
        if (lcm->enabled) {
            // Copy rest of payload into data for LCM to pull
            lcm->payload_size = min(len - idx, MAX_LCM_PAYLOAD_LENGTH);
            for (int i = 0; i < lcm->payload_size; i++) {
                lcm->payload[i] = cfg[idx + i];
            }
        } else {
            if (len > 5) {
                // d->float_conf.led_mode = cfg[idx++];
                // d->float_conf.led_mode_idle = cfg[idx++];
                // d->float_conf.led_status_mode = cfg[idx++];
            }
        }
    }
}

static uint8_t clamp_u8(uint8_t value, uint8_t max) {
    return value > max ? max : value;
}

// Re-Float Rink-Mixed (rf72): Refloat's LED colors as SLCM hues (degrees / 2;
// 0xFF = white / black: no hue, the SLCM's own color is used)
static const uint8_t refloat_color_hue[] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0,  7,   9,   5,   10,  13,  13,  14,  19,  18,  30,  41,
    46,   60,   69,   83,   90, 107, 100, 101, 107, 120, 135, 132, 150, 157, 167, 170,
};

static uint8_t refloat_hue(LedColor c) {
    return (unsigned) c < sizeof(refloat_color_hue) ? refloat_color_hue[c] : 0xFF;
}

// Refloat's Status Idle animation as the closest SLCM idle animation; the
// fill / scan ones are built as a CUSTOM animation (custom[8], see the SLCM's
// status_leds.c). Refloat's idle animation off (timeout 0, its default) =
// STANDARD, the battery display, as on a stock LCM.
static uint8_t refloat_idle_anim(const LcmData *lcm, uint8_t custom[LCM_VP_CUSTOM_LENGTH]) {
    const CfgLeds *cfg = lcm->leds_cfg;
    if (!cfg || cfg->status.idle_timeout == 0) {
        return 0;
    }
    const LedBar *bar = &cfg->status_idle;
    uint8_t a = refloat_hue(bar->color1), b = refloat_hue(bar->color2);
    bool white = a == 0xFF;
    if (white) {
        a = b == 0xFF ? 0 : b;
    }
    if (b == 0xFF) {
        b = a;
    }
    // Refloat's animation time runs at `speed` cycles per second
    float period = bar->speed > 0.01f ? 1.0f / bar->speed : 12.8f;
    uint8_t steps = (uint8_t) fminf(fmaxf(period / 0.05f - 1.0f, 0.0f), 255.0f);
    // [0] bit0 engine (0 fill, 1 scan), bits1-3 colors (0 hue up, 2 wave A-B,
    // 4 my color), bits4-6 pattern; [1] brightness (0 steady, 1 breathe,
    // 2 flash); [2], [3] hues; [4] color speed, [5] motion speed; [6] lowest
    const uint8_t solid_colors = white ? 4 : 2;  // white: the SLCM's own color
    uint8_t c0 = 0, c1 = 0;
    switch (bar->mode) {
    case LED_ANIM_SOLID:
        c0 = solid_colors << 1;
        b = a;
        break;
    case LED_ANIM_FADE:  // fade between the two colors
        c0 = 2 << 1;
        break;
    case LED_ANIM_PULSE:  // a moving pulse in color 1
        c0 = 1 | (solid_colors << 1);
        b = a;
        break;
    case LED_ANIM_STROBE:
        c0 = solid_colors << 1;
        c1 = 2;
        b = a;
        break;
    case LED_ANIM_KNIGHT_RIDER:  // a bouncing scanner in color 1
        c0 = 1 | (solid_colors << 1) | (6 << 4);
        b = a;
        break;
    case LED_ANIM_FELONY:
        return 5;  // THE FUZZ
    case LED_ANIM_RAINBOW_CYCLE:
    case LED_ANIM_RAINBOW_FADE:  // whole bar through the rainbow
        a = 0;
        b = 179;
        break;
    case LED_ANIM_RAINBOW_ROLL:  // rainbow rolling along the bar
        c0 = 1 << 4;
        a = 0;
        b = 179;
        break;
    default:
        return 0;
    }
    custom[0] = c0;
    custom[1] = c1;
    custom[2] = a;
    custom[3] = b;
    custom[4] = steps;
    custom[5] = steps;
    custom[6] = 0;
    custom[7] = 0;
    return 14;  // CUSTOM
}

bool lcm_vp_set(LcmData *lcm, const uint8_t *buffer, int len) {
    if (len < LCM_VP_SETTINGS_LENGTH - 1) {
        return false;
    }

    LcmVpSettings vp;
    memset(&vp, 0, sizeof(vp));  // padding too, for the memcmp below
    vp.valid = true;
    vp.flags = buffer[0] & LCM_VP_STORED_FLAGS;
    vp.headlight_brightness = clamp_u8(buffer[1], 100);
    vp.status_idle_brightness = clamp_u8(buffer[2], 100);
    vp.status_riding_brightness = clamp_u8(buffer[3], 100);
    for (int i = 0; i < LCM_VP_ANIMATIONS; ++i) {
        vp.animations[i] = buffer[4 + i] == LCM_VP_ANIM_REFLOAT && i == 0
            ? LCM_VP_ANIM_REFLOAT
            : clamp_u8(buffer[4 + i], LCM_VP_ANIMATION_COUNT - 1);
    }
    vp.color = ((uint16_t) buffer[9] << 8) | buffer[10];
    if (vp.color > 359) {
        vp.color = 359;
    }
    vp.headlight_idle_brightness =
        len >= LCM_VP_SETTINGS_LENGTH ? clamp_u8(buffer[11], 100) : vp.headlight_brightness;
    // CUSTOM animation: from rf71 apps; older ones keep what's there
    if (len >= LCM_VP_SETTINGS_LENGTH + LCM_VP_CUSTOM_LENGTH) {
        memcpy(vp.custom, &buffer[LCM_VP_SETTINGS_LENGTH], LCM_VP_CUSTOM_LENGTH);
    } else {
        memcpy(vp.custom, lcm->vp.custom, LCM_VP_CUSTOM_LENGTH);
    }

    vp.snooze_minutes = len >= LCM_VP_SETTINGS_LENGTH + LCM_VP_CUSTOM_LENGTH + 1
        ? clamp_u8(buffer[LCM_VP_SETTINGS_LENGTH + LCM_VP_CUSTOM_LENGTH], LCM_VP_SNOOZE_MAX)
        : lcm->vp.snooze_minutes;

    bool changed = memcmp(&vp, &lcm->vp, sizeof(vp)) != 0;
    lcm->vp = vp;
    return changed;
}

void lcm_vp_pack(const LcmData *lcm, uint32_t words[LCM_VP_EEPROM_WORDS]) {
    const LcmVpSettings *vp = &lcm->vp;
    // word 0: magic 0x4C ('L'), flags, headlight riding %, status idle %
    words[0] = (0x4Cu << 24) | ((uint32_t) vp->flags << 16) |
        ((uint32_t) vp->headlight_brightness << 8) | vp->status_idle_brightness;
    // word 1: status riding %, idle, riding, dozing animations
    words[1] = ((uint32_t) vp->status_riding_brightness << 24) |
        ((uint32_t) vp->animations[0] << 16) | ((uint32_t) vp->animations[1] << 8) |
        vp->animations[2];
    // word 2: boot, shutdown animations, color
    words[2] =
        ((uint32_t) vp->animations[3] << 24) | ((uint32_t) vp->animations[4] << 16) | vp->color;
    // word 3: magic 0x49 ('I'), -, snooze minutes (rf86), headlight idle %
    words[3] = (0x49u << 24) | ((uint32_t) vp->snooze_minutes << 8) | vp->headlight_idle_brightness;
    // words 4, 5: CUSTOM animation
    for (int i = 0; i < 2; ++i) {
        words[4 + i] = ((uint32_t) vp->custom[4 * i] << 24) |
            ((uint32_t) vp->custom[4 * i + 1] << 16) | ((uint32_t) vp->custom[4 * i + 2] << 8) |
            vp->custom[4 * i + 3];
    }
}

void lcm_vp_unpack(LcmData *lcm, const uint32_t words[LCM_VP_EEPROM_WORDS]) {
    if ((words[0] >> 24) != 0x4C) {
        return;
    }
    uint8_t buffer[LCM_VP_SETTINGS_LENGTH + LCM_VP_CUSTOM_LENGTH + 1] = {
        (words[0] >> 16) & 0xFF, (words[0] >> 8) & 0xFF,  words[0] & 0xFF,        words[1] >> 24,
        (words[1] >> 16) & 0xFF, (words[1] >> 8) & 0xFF,  words[1] & 0xFF,        words[2] >> 24,
        (words[2] >> 16) & 0xFF, (words[2] >> 8) & 0xFF,  words[2] & 0xFF,        words[3] & 0xFF,
        words[4] >> 24,          (words[4] >> 16) & 0xFF, (words[4] >> 8) & 0xFF, words[4] & 0xFF,
        words[5] >> 24,          (words[5] >> 16) & 0xFF, (words[5] >> 8) & 0xFF, words[5] & 0xFF,
        (words[3] >> 8) & 0xFF,
    };
    // saved before the idle headlight level existed: idle = riding (and no
    // CUSTOM animation yet either)
    int len = (words[3] >> 24) == 0x49 ? LCM_VP_SETTINGS_LENGTH + LCM_VP_CUSTOM_LENGTH + 1
                                       : LCM_VP_SETTINGS_LENGTH - 1;
    lcm_vp_set(lcm, buffer, len);
}

void lcm_vp_test_beep(LcmData *lcm) {
    lcm->vp_test_beep = !lcm->vp_test_beep;
}
