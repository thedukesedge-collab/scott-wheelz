/*
 * Re-Float Rink-Mixed: status LED tests for the VP_LIGHT_CONTROL build.
 * (Stock tests run in test_status_leds; config mode is compiled out here.)
 * Checks the VP battery gauge colors:
 * white above 35 %, yellow 35-20 %, red 20-5 % (solid gauge), and the
 * stock flashing red at 5 % and below. GPL-3.0, as the rest of ALCM.
 */
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <cmocka.h>

#include "test_status_leds.h"

// Colors are {G, R, B}
static const uint8_t vp_white[3] = {0xff, 0xff, 0xff};
static const uint8_t vp_yellow[3] = {0xc0, 0xff, 0x00};
static const uint8_t vp_red[3] = {0x00, 0xff, 0x00};

static int check_rgb(const LargestIntegralType value, const LargestIntegralType expected)
{
    return memcmp((const void *)(uintptr_t)value, (const void *)(uintptr_t)expected, 3) == 0;
}

static void expect_gauge(const uint8_t *rgb)
{
    expect_function_call(stop_animation);
    expect_any(scan_animation_setup, buffer);
    expect_any(scan_animation_setup, direction);
    expect_any(scan_animation_setup, color_mode);
    expect_any(scan_animation_setup, movement_speed);
    expect_any(scan_animation_setup, hue_min);
    expect_any(scan_animation_setup, hue_max);
    expect_any(scan_animation_setup, color_speed);
    expect_any(scan_animation_setup, scan_start);
    expect_any(scan_animation_setup, scan_end);
    expect_any(scan_animation_setup, init_mu);
    expect_check(scan_animation_setup, rgb, check_rgb, (uintptr_t)rgb);
    expect_function_call(scan_animation_setup);
    will_return(scan_animation_setup, 1U);
}

static void test_vp_battery_colors(void **state)
{
    (void)state;
    expect_gauge(vp_white);
    display_battery(800); // 80 %: 8 LEDs, white
    expect_gauge(vp_white);
    display_battery(351);
    expect_gauge(vp_yellow);
    display_battery(350); // 35 %: yellow
    expect_gauge(vp_yellow);
    display_battery(201);
    expect_gauge(vp_red);
    display_battery(200); // 20 %: red
    expect_gauge(vp_red);
    display_battery(51);
}

static const struct CMUnitTest status_leds_vp_tests[] = {
    cmocka_unit_test_setup(test_vp_battery_colors, test_status_leds_setup),
};

int main(void)
{
    int result = 0;
    result += cmocka_run_group_tests_name("Status LEDs VP Test", status_leds_vp_tests, NULL, NULL);
    return result;
}
