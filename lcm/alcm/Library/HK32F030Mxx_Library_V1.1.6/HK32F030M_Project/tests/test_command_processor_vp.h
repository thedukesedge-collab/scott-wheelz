/*
 * This file is part of Advanced LCM (ALCM) project (GPL-3.0-or-later).
 *
 * Tests for the VP_LIGHT_CONTROL build (Refloat Glow): lights are
 * only set from the app, the button keeps only its power functions.
 */
#ifndef TEST_COMMAND_PROCESSOR_VP_H
#define TEST_COMMAND_PROCESSOR_VP_H

#include <cmocka.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>

#include "command_processor.h"
#include "event_queue.h"
#include "board_mode.h"
#include "footpads.h"
#include "settings.h"
#include "mock_event_queue.h"
#include "mock_timer.h"
#include "mock_settings.h"

void command_processor_default_handler(event_type_t event, const event_data_t *data);
EVENT_HANDLER(command_processor, button);
EVENT_HANDLER(command_processor, board_mode);

static int cpvp_setup(void **state)
{
    (void)state;

    event_queue_init();
    timer_init();
    settings_init();

    expect_value(subscribe_event, event, EVENT_BUTTON_CLICK);
    expect_any(subscribe_event, callback);
    expect_value(subscribe_event, event, EVENT_BUTTON_HOLD);
    expect_any(subscribe_event, callback);
    expect_value(subscribe_event, event, EVENT_BUTTON_UP);
    expect_any(subscribe_event, callback);
    expect_value(subscribe_event, event, EVENT_FOOTPAD_CHANGED);
    expect_any(subscribe_event, callback);
    expect_value(subscribe_event, event, EVENT_BOARD_MODE_CHANGED);
    expect_any(subscribe_event, callback);

    assert_int_equal(LCM_SUCCESS, command_processor_init());
    return 0;
}

static event_data_t cpvp_count(uint8_t count)
{
    event_data_t data = {0};
    data.click_count = count;
    return data;
}

/* Clicks (1, 2, 3, 4) never touch the light/beeper settings or push events */
static void test_vp_clicks_do_nothing(void **state)
{
    (void)state;

    bool_t headlights = settings_get()->enable_headlights;
    bool_t leds = settings_get()->enable_status_leds;
    bool_t beep = settings_get()->enable_beep;

    for (uint8_t count = 1U; count <= 4U; count++)
    {
        event_data_t data = cpvp_count(count);

        will_return(board_mode_get, BOARD_MODE_IDLE);
        if (count >= 2U)
        {
            // a click series goes to vesc_serial (VP: reported to the
            // package; stock: refused while riding), no ACK here
            expect_value(vesc_serial_stock_level_step, clicks, count);
            will_return(vesc_serial_stock_level_step, false);
        }
        command_processor_button_event_handler(EVENT_BUTTON_CLICK, &data);
    }

    assert_int_equal(headlights, settings_get()->enable_headlights);
    assert_int_equal(leds, settings_get()->enable_status_leds);
    assert_int_equal(beep, settings_get()->enable_beep);
}

/* Click-click on stock Refloat: the light level steps, with an ACK */
static void test_vp_click_click_stock_level(void **state)
{
    (void)state;

    event_data_t data = cpvp_count(2);

    will_return(board_mode_get, BOARD_MODE_IDLE);
    expect_value(vesc_serial_stock_level_step, clicks, 2);
    will_return(vesc_serial_stock_level_step, true);
    expect_value(event_queue_push, event, EVENT_COMMAND_ACK);
    expect_any(event_queue_push, data);
    command_processor_button_event_handler(EVENT_BUTTON_CLICK, &data);
}

/* Hold: shuts the board down - the power function stays */
static void test_vp_hold_shuts_down(void **state)
{
    (void)state;

    event_data_t data = cpvp_count(1);

    will_return(board_mode_get, BOARD_MODE_IDLE);
    expect_value(event_queue_push, event, EVENT_COMMAND_SHUTDOWN);
    expect_any(event_queue_push, data);
    command_processor_button_event_handler(EVENT_BUTTON_HOLD, &data);
}

/* Hold also shuts down while riding (same as stock) */
static void test_vp_hold_shuts_down_riding(void **state)
{
    (void)state;

    event_data_t data = cpvp_count(1);

    will_return(board_mode_get, BOARD_MODE_RIDING);
    expect_value(event_queue_push, event, EVENT_COMMAND_SHUTDOWN);
    expect_any(event_queue_push, data);
    command_processor_button_event_handler(EVENT_BUTTON_HOLD, &data);
}

/* Click-hold no longer enters config mode */
static void test_vp_click_hold_no_config(void **state)
{
    (void)state;

    event_data_t data = cpvp_count(2);

    will_return(board_mode_get, BOARD_MODE_IDLE);
    expect_value(event_queue_push, event, EVENT_COMMAND_NACK);
    expect_any(event_queue_push, data);
    command_processor_button_event_handler(EVENT_BUTTON_HOLD, &data);
}

/* Footpads never change settings */
static void test_vp_footpads_do_nothing(void **state)
{
    (void)state;

    event_data_t data = {0};
    data.footpads_state = LEFT_FOOTPAD;

    will_return(board_mode_get, BOARD_MODE_IDLE);
    command_processor_button_event_handler(EVENT_FOOTPAD_CHANGED, &data);
}

/* The factory reset gesture still works */
static void test_vp_factory_reset_gesture(void **state)
{
    (void)state;

    will_return(button_driver_hw_is_pressed, true);
    expect_function_call(settings_reset);
    expect_value(event_queue_push, event, EVENT_COMMAND_SHUTDOWN);
    expect_any(event_queue_push, data);

    event_data_t data = {0};
    data.board_mode.previous_mode = BOARD_MODE_BOOTING;
    data.board_mode.mode = BOARD_MODE_IDLE;
    data.board_mode.submode = BOARD_SUBMODE_IDLE_ACTIVE;
    command_processor_board_mode_event_handler(EVENT_BOARD_MODE_CHANGED, &data);
}

/* Button input is still ignored while booting / off / fault / locked */
static void test_vp_hold_ignored_when_off(void **state)
{
    (void)state;

    event_data_t data = cpvp_count(1);

    will_return(board_mode_get, BOARD_MODE_OFF);
    command_processor_button_event_handler(EVENT_BUTTON_HOLD, &data);
}

const struct CMUnitTest command_processor_vp_tests[] = {
    cmocka_unit_test_setup(test_vp_clicks_do_nothing, cpvp_setup),
    cmocka_unit_test_setup(test_vp_click_click_stock_level, cpvp_setup),
    cmocka_unit_test_setup(test_vp_hold_shuts_down, cpvp_setup),
    cmocka_unit_test_setup(test_vp_hold_shuts_down_riding, cpvp_setup),
    cmocka_unit_test_setup(test_vp_click_hold_no_config, cpvp_setup),
    cmocka_unit_test_setup(test_vp_footpads_do_nothing, cpvp_setup),
    cmocka_unit_test_setup(test_vp_factory_reset_gesture, cpvp_setup),
    cmocka_unit_test_setup(test_vp_hold_ignored_when_off, cpvp_setup),
};

#endif
