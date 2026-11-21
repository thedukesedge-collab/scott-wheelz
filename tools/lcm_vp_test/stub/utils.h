// SPDX-License-Identifier: GPL-3.0-or-later
// Part of Refloat Glow (formerly Refloat rink-mixed), 2026. See FORK_NOTES.md.

#pragma once
void log_error(const char *f, ...);
void fatal_error_terminate(void);
#define SEND_APP_DATA(buffer, buf_size, ind) VESC_IF->send_app_data(buffer, ind)
// same as src/utils.h
#define min(a, b)                                                                                  \
    ({                                                                                             \
        __typeof__(a) _a = (a);                                                                    \
        __typeof__(b) _b = (b);                                                                    \
        _a < _b ? _a : _b;                                                                         \
    })
#define max(a, b)                                                                                  \
    ({                                                                                             \
        __typeof__(a) _a = (a);                                                                    \
        __typeof__(b) _b = (b);                                                                    \
        _a > _b ? _a : _b;                                                                         \
    })
