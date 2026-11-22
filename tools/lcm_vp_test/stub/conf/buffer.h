// SPDX-License-Identifier: GPL-3.0-or-later
// Part of Refloat Glow, 2026. See FORK_NOTES.md.

#pragma once
#include <stdint.h>
static inline void buffer_append_float16(uint8_t *b, float n, float s, int32_t *i) { int16_t v = (int16_t)(n * s); b[(*i)++] = v >> 8; b[(*i)++] = v & 0xFF; }
static inline void buffer_append_float32_auto(uint8_t *b, float n, int32_t *i) { (void)b; (void)n; *i += 4; }
