// SPDX-License-Identifier: GPL-3.0-or-later
// Part of Refloat rink-mixed (refloatRM), 2026. See FORK_NOTES.md.
// Re-Float Rink-Mixed: battery curve for the LCM status bar. See
// battery_curve.h. GPL-3.0-or-later.

#include "battery_curve.h"

#include <string.h>

#define FILTER_ALPHA 0.1f  // per LCM poll (4 Hz): ~2.5 s

void battery_curve_init(BatteryCurve *bc) {
    memset(bc, 0, sizeof(*bc));
    bc->percent = BATTERY_CURVE_NONE;
}

static bool points_valid(const uint8_t *p) {
    for (int i = 1; i < BATTERY_CURVE_POINTS; i++) {
        if (p[i] >= p[i - 1]) {
            return false;
        }
    }
    return true;
}

bool battery_curve_set(BatteryCurve *bc, const uint8_t *buf, int len) {
    if (len < BATTERY_CURVE_SET_LENGTH || !points_valid(&buf[2])) {
        return false;
    }
    bool enabled = buf[0] != 0;
    bool changed = enabled != bc->enabled || buf[1] != bc->resistance ||
        memcmp(bc->points, &buf[2], BATTERY_CURVE_POINTS) != 0;
    bc->enabled = enabled;
    bc->resistance = buf[1];
    memcpy(bc->points, &buf[2], BATTERY_CURVE_POINTS);
    if (!enabled) {
        bc->percent = BATTERY_CURVE_NONE;
    }
    return changed;
}

void battery_curve_pack(const BatteryCurve *bc, uint32_t w[4]) {
    const uint8_t *p = bc->points;
    w[0] = (0x42u << 24) | ((uint32_t) bc->enabled << 16) | ((uint32_t) bc->resistance << 8) | p[0];
    w[1] = ((uint32_t) p[1] << 24) | ((uint32_t) p[2] << 16) | ((uint32_t) p[3] << 8) | p[4];
    w[2] = ((uint32_t) p[5] << 24) | ((uint32_t) p[6] << 16) | ((uint32_t) p[7] << 8) | p[8];
    w[3] = ((uint32_t) p[9] << 24) | ((uint32_t) p[10] << 16);
}

void battery_curve_unpack(BatteryCurve *bc, const uint32_t w[4]) {
    if ((w[0] >> 24) != 0x42) {
        return;
    }
    uint8_t buf[BATTERY_CURVE_SET_LENGTH] = {
        (w[0] >> 16) & 1,
        (w[0] >> 8) & 0xFF,
        w[0] & 0xFF,
        w[1] >> 24,
        (w[1] >> 16) & 0xFF,
        (w[1] >> 8) & 0xFF,
        w[1] & 0xFF,
        w[2] >> 24,
        (w[2] >> 16) & 0xFF,
        (w[2] >> 8) & 0xFF,
        w[2] & 0xFF,
        w[3] >> 24,
        (w[3] >> 16) & 0xFF,
    };
    battery_curve_set(bc, buf, BATTERY_CURVE_SET_LENGTH);
}

uint8_t battery_curve_update(BatteryCurve *bc, float v_pack, float i_in, int cells) {
    if (!bc->enabled || cells <= 0 || v_pack <= 0.0f) {
        bc->percent = BATTERY_CURVE_NONE;
        return bc->percent;
    }

    // sag compensation: the open-circuit voltage is higher than the terminal
    // voltage by I * R while drawing current (lower while regenerating)
    float v = v_pack + i_in * (bc->resistance * 0.002f);
    if (!bc->filter_init) {
        bc->v_filtered = v;
        bc->filter_init = true;
    } else {
        bc->v_filtered += (v - bc->v_filtered) * FILTER_ALPHA;
    }

    float mv = bc->v_filtered * 1000.0f / cells;
    const uint8_t *p = bc->points;
    float pct;
    if (mv >= 2500.0f + 10.0f * p[0]) {
        pct = 100.0f;
    } else if (mv <= 2500.0f + 10.0f * p[BATTERY_CURVE_POINTS - 1]) {
        pct = 0.0f;
    } else {
        int i = 0;
        while (mv < 2500.0f + 10.0f * p[i + 1]) {
            i++;
        }
        float hi = 2500.0f + 10.0f * p[i], lo = 2500.0f + 10.0f * p[i + 1];
        pct = 100.0f - 10.0f * i - 10.0f * (hi - mv) / (hi - lo);
    }
    bc->percent = (uint8_t) (pct + 0.5f);
    return bc->percent;
}
