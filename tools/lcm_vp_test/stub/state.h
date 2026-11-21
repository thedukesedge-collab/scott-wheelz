// SPDX-License-Identifier: GPL-3.0-or-later
// Part of Refloat rink-mixed (refloatRM), 2026. See FORK_NOTES.md.

#pragma once
#include <stdint.h>
typedef enum { STATE_DISABLED = 0, STATE_STARTUP, STATE_READY, STATE_RUNNING } RunState;
typedef enum { MODE_NORMAL = 0, MODE_HANDTEST } Mode;
typedef struct { RunState state; Mode mode; } State;
static inline uint8_t state_compat(const State *s) { return s->state == STATE_RUNNING ? 1 : 0; }
