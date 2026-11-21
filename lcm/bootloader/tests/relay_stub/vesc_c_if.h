// SPDX-License-Identifier: GPL-3.0-or-later
// Part of Refloat rink-mixed (refloatRM), 2026. See FORK_NOTES.md.

#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct {
    void (*send_app_data)(unsigned char *, unsigned int);
} vesc_c_if;
extern vesc_c_if *VESC_IF;
