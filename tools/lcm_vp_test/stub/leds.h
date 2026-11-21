// SPDX-License-Identifier: GPL-3.0-or-later
// Part of Refloat Glow (formerly Refloat rink-mixed), 2026. See FORK_NOTES.md.

#pragma once
#include <stdbool.h>
#include <stdint.h>
#define LED_MODE_EXTERNAL 0x2
typedef struct { uint8_t mode; } CfgHwLeds;
typedef enum { COLOR_BLACK = 0, COLOR_WHITE_FULL, COLOR_WHITE_RGB, COLOR_WHITE_SINGLE, COLOR_RED,
    COLOR_BLUE = 25 } LedColor;
typedef enum { LED_ANIM_SOLID = 0, LED_ANIM_FADE, LED_ANIM_PULSE, LED_ANIM_STROBE,
    LED_ANIM_KNIGHT_RIDER, LED_ANIM_FELONY, LED_ANIM_RAINBOW_CYCLE, LED_ANIM_RAINBOW_FADE,
    LED_ANIM_RAINBOW_ROLL } LedAnimMode;
/* same field names as src/conf/datatypes.h */
typedef struct { float brightness; LedColor color1, color2; LedAnimMode mode; float speed; } LedBar;
typedef struct { uint16_t idle_timeout; float brightness_headlights_on, brightness_headlights_off; } StatusBar;
typedef struct { LedBar headlights, front; StatusBar status; LedBar status_idle; bool lights_off_when_lifted; } CfgLeds;
typedef struct { bool enabled, headlights_enabled; } LedsRuntimeStatus;
typedef struct { const CfgLeds *cfg; LedsRuntimeStatus rs; } Leds;
static inline const LedsRuntimeStatus *leds_get_runtime_status(const Leds *l) { return &l->rs; }
