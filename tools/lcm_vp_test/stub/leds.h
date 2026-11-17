#pragma once
#include <stdbool.h>
#include <stdint.h>
#define LED_MODE_EXTERNAL 0x2
typedef struct { uint8_t mode; } CfgHwLeds;
typedef struct { float brightness; } CfgBright;
typedef struct { float brightness_headlights_on, brightness_headlights_off; } CfgStatus;
typedef struct { CfgBright headlights, front; CfgStatus status; bool lights_off_when_lifted; } CfgLeds;
typedef struct { bool enabled, headlights_enabled; } LedsRuntimeStatus;
typedef struct { const CfgLeds *cfg; LedsRuntimeStatus rs; } Leds;
static inline const LedsRuntimeStatus *leds_get_runtime_status(const Leds *l) { return &l->rs; }
