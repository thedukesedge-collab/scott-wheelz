// SPDX-License-Identifier: GPL-3.0-or-later
// Part of Refloat Glow, 2026. See FORK_NOTES.md.

#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
typedef struct { int (*mc_get_fault)(void); float (*mc_get_tot_current_in)(void); float (*mc_get_input_voltage_filtered)(void);
  void (*send_app_data)(unsigned char *, unsigned int); float (*mc_get_battery_level)(float *); } vesc_c_if;
extern vesc_c_if *VESC_IF;
