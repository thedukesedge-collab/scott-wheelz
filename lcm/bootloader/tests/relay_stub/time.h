// SPDX-License-Identifier: GPL-3.0-or-later
// Part of Refloat Glow (formerly Refloat rink-mixed), 2026. See FORK_NOTES.md.

#pragma once
#include "state.h"
#include "vesc_c_if.h"
#define SYSTEM_TICK_RATE_HZ 10000
typedef uint32_t refloat_time_t;
#define time_t refloat_time_t /* the package's time_t, not the host's */
typedef struct { time_t now; } Time;
static inline bool timer_older(const Time *t, time_t timer, float seconds) {
    return t->now - timer > (time_t)(seconds * SYSTEM_TICK_RATE_HZ);
}
