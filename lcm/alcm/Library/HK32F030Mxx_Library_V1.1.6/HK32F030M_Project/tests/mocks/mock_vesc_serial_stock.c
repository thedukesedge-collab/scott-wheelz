/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Part of Refloat Glow (SLCM), 2026. See FORK_NOTES.md.
 * Mock of vesc_serial_stock_level_step() for the VP command processor tests.
 */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include "vesc_serial.h"

bool_t vesc_serial_stock_level_step(uint8_t clicks)
{
    check_expected(clicks);
    return (bool_t)mock();
}
