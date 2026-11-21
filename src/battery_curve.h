// SPDX-License-Identifier: GPL-3.0-or-later
// Part of Refloat rink-mixed (refloatRM), 2026. See FORK_NOTES.md.
// Re-Float Rink-Mixed: battery % for the LCM status bar from a per-cell
// voltage curve chosen in the app (presets from floatwheel's own tables, or
// a custom one). Computed here and sent to the LCM in the VP block, so the
// LCM firmware needs no curve of its own. GPL-3.0-or-later.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define BATTERY_CURVE_POINTS 11  // cell voltage at 100, 90, ... 0 %
#define BATTERY_CURVE_SET_LENGTH 13  // enabled, resistance, 11 points
#define BATTERY_CURVE_NONE 0xFF  // no curve: the LCM uses the VESC's battery %

typedef struct {
    bool enabled;
    uint8_t resistance;  // pack resistance for sag compensation, 2 mOhm steps
    uint8_t points[BATTERY_CURVE_POINTS];  // cell mV = 2500 + 10 * point, falling
    float v_filtered;  // compensated pack voltage, smoothed
    bool filter_init;
    uint8_t percent;  // last result, or BATTERY_CURVE_NONE
} BatteryCurve;

void battery_curve_init(BatteryCurve *bc);

// From the app: enabled, resistance, 11 points. Rejects curves that don't
// fall strictly from 100 % to 0 %. Returns true if anything changed.
bool battery_curve_set(BatteryCurve *bc, const uint8_t *buf, int len);

// 4 EEPROM words (magic 0x42 in word 0)
void battery_curve_pack(const BatteryCurve *bc, uint32_t words[4]);
void battery_curve_unpack(BatteryCurve *bc, const uint32_t words[4]);

// New sample (pack voltage, battery current, series cell count); returns
// the battery % (0-100), or BATTERY_CURVE_NONE when off / unknown cells.
uint8_t battery_curve_update(BatteryCurve *bc, float v_pack, float i_in, int cells);
