/*
 * Refloat Glow: board_mode tests for the VP_LIGHT_CONTROL build.
 * Runs the stock board_mode tests plus the VESC comm-loss behavior: like
 * stock, a lost link is always a fault (the idle shutdown timer is
 * cancelled, power stays on), but it clears itself when the VESC answers
 * again, and it is silent when it began while not riding.
 * GPL-3.0, as the rest of ALCM.
 */
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <cmocka.h>

#include "test_board_mode.h"

void board_mode_boot_timer_handler(uint32_t system_tick); // board_mode.c (VP)

static void comm_timeout(void)
{
    event_data_t d = {0};
    d.emergency_fault = EMERGENCY_FAULT_VESC_COMM_TIMEOUT;
    event_queue_call_mocked_callback(EVENT_EMERGENCY_FAULT, &d);
}

void test_vp_comm_loss_idle_faults_quietly_then_clears(void **state)
{
    (void)state;
    event_data_t null_event_data = {0};
    board_mode_event_data_t expected_state = {0};
    board_mode_to_idle();

    // A fault, as in stock: the idle shutdown timer is cancelled
    expect_value(event_queue_push, event, EVENT_BOARD_MODE_CHANGED);
    expected_state.mode = BOARD_MODE_FAULT;
    expected_state.submode = BOARD_SUBMODE_FAULT_INTERNAL;
    expect_check(event_queue_push, data, validate_board_mode_event_data,
                 (uintmax_t)&expected_state);
    expect_any(is_timer_active, timer_id);
    will_return(is_timer_active, true);
    expect_any(cancel_timer, timer_id);
    will_return(cancel_timer, LCM_SUCCESS);
    comm_timeout();
    assert_int_equal(board_mode_get(), BOARD_MODE_FAULT);
    assert_true(board_mode_fault_is_quiet()); // no fault beeping

    // Footpads / RPM do nothing in fault mode (no idle countdown either)
    event_queue_call_mocked_callback(EVENT_FOOTPAD_CHANGED, &null_event_data);
    assert_int_equal(board_mode_get(), BOARD_MODE_FAULT);

    // VESC answers again: back to idle, timer restarted
    expect_value(event_queue_push, event, EVENT_BOARD_MODE_CHANGED);
    expected_state.mode = BOARD_MODE_IDLE;
    expected_state.submode = BOARD_SUBMODE_IDLE_ACTIVE;
    expect_check(event_queue_push, data, validate_board_mode_event_data,
                 (uintmax_t)&expected_state);
    expect_any(set_timer, timeout);
    expect_any(set_timer, callback);
    expect_any(set_timer, repeat);
    event_queue_call_mocked_callback(EVENT_VESC_ALIVE, &null_event_data);
    assert_int_equal(board_mode_get(), BOARD_MODE_IDLE);
    assert_false(board_mode_fault_is_quiet());
}

void test_vp_comm_loss_riding_faults_then_clears(void **state)
{
    (void)state;
    event_data_t null_event_data = {0};
    board_mode_event_data_t expected_state = {0};
    board_mode_to_idle();
    step_on_board();

    expect_value(event_queue_push, event, EVENT_BOARD_MODE_CHANGED);
    expected_state.mode = BOARD_MODE_FAULT;
    expected_state.submode = BOARD_SUBMODE_FAULT_INTERNAL;
    expect_check(event_queue_push, data, validate_board_mode_event_data,
                 (uintmax_t)&expected_state);
    expect_any(is_timer_active, timer_id);
    will_return(is_timer_active, false);
    comm_timeout();
    assert_int_equal(board_mode_get(), BOARD_MODE_FAULT);
    assert_int_equal(board_submode_get(), BOARD_SUBMODE_FAULT_INTERNAL);
    assert_false(board_mode_fault_is_quiet()); // mid-ride: beeps

    expect_value(event_queue_push, event, EVENT_BOARD_MODE_CHANGED);
    expected_state.mode = BOARD_MODE_IDLE;
    expected_state.submode = BOARD_SUBMODE_IDLE_ACTIVE;
    expect_check(event_queue_push, data, validate_board_mode_event_data,
                 (uintmax_t)&expected_state);
    expect_any(set_timer, timeout);
    expect_any(set_timer, callback);
    expect_any(set_timer, repeat);
    event_queue_call_mocked_callback(EVENT_VESC_ALIVE, &null_event_data);
    assert_int_equal(board_mode_get(), BOARD_MODE_IDLE);
    assert_int_equal(board_submode_get(), BOARD_SUBMODE_IDLE_ACTIVE);
}

void test_vp_other_fault_still_latches(void **state)
{
    (void)state;
    event_data_t null_event_data = {0};
    board_mode_to_idle();
    trigger_emergency_fault();
    // A real internal fault is not cleared by the VESC answering
    event_queue_call_mocked_callback(EVENT_VESC_ALIVE, &null_event_data);
    assert_int_equal(board_mode_get(), BOARD_MODE_FAULT);
}

static board_mode_event_data_t vp_expected;
static void vp_expect_mode(board_mode_t mode, board_submode_t submode)
{
    vp_expected.mode = mode;
    vp_expected.submode = submode;
    expect_value(event_queue_push, event, EVENT_BOARD_MODE_CHANGED);
    expect_check(event_queue_push, data, validate_board_mode_event_data,
                 (uintmax_t)&vp_expected);
}

static void vp_expect_timer(void)
{
    expect_any(set_timer, timeout);
    expect_any(set_timer, callback);
    expect_any(set_timer, repeat);
}

/* Refloat Glow: booting ends after 4 s with no VESC answer (like Dado's
 * firmware), so the normal hold-to-off always works; an answer that comes
 * first ends it sooner and the timer then does nothing */
void test_vp_boot_timer(void **state)
{
    (void)state;
    event_data_t d = {0};

    vp_expect_mode(BOARD_MODE_BOOTING, BOARD_SUBMODE_UNDEFINED);
    expect_value(set_timer, timeout, BOOT_TIMEOUT);
    expect_any(set_timer, callback);
    expect_value(set_timer, repeat, false);
    event_queue_call_mocked_callback(EVENT_COMMAND_BOOT, &d);

    // 4 s, no answer: idle
    vp_expect_mode(BOARD_MODE_IDLE, BOARD_SUBMODE_IDLE_ACTIVE);
    vp_expect_timer();
    board_mode_boot_timer_handler(4000);
    assert_int_equal(board_mode_get(), BOARD_MODE_IDLE);

    // a late timer call (answer came first) changes nothing
    board_mode_boot_timer_handler(4000);
    assert_int_equal(board_submode_get(), BOARD_SUBMODE_IDLE_ACTIVE);
}

/* Refloat Glow: a hold-to-off begun while locked goes back to locked when
 * the button is let go early */
void test_vp_hold_off_locked_abort(void **state)
{
    (void)state;
    event_data_t d = {0};

    board_mode_to_idle(); // -> IDLE/ACTIVE, idle timer running

    d.enable = true;
    vp_expect_mode(BOARD_MODE_DISABLED, BOARD_SUBMODE_UNDEFINED);
    expect_any(is_timer_active, timer_id);
    will_return(is_timer_active, true);
    expect_any(cancel_timer, timer_id);
    will_return(cancel_timer, LCM_SUCCESS);
    event_queue_call_mocked_callback(EVENT_VESC_LOCKED_CHANGED, &d);
    d.enable = false;

    vp_expect_mode(BOARD_MODE_IDLE, BOARD_SUBMODE_IDLE_SHUTTING_DOWN);
    vp_expect_timer();
    event_queue_call_mocked_callback(EVENT_COMMAND_SHUTDOWN, &d);

    // let go early: back to locked (the shutdown timer is cancelled)
    vp_expect_mode(BOARD_MODE_DISABLED, BOARD_SUBMODE_UNDEFINED);
    expect_any(is_timer_active, timer_id);
    will_return(is_timer_active, true);
    expect_any(cancel_timer, timer_id);
    will_return(cancel_timer, LCM_SUCCESS);
    event_queue_call_mocked_callback(EVENT_BUTTON_UP, &d);
    assert_int_equal(board_mode_get(), BOARD_MODE_DISABLED);

    // unlocked: normal idle again
    vp_expect_mode(BOARD_MODE_IDLE, BOARD_SUBMODE_IDLE_ACTIVE);
    vp_expect_timer();
    event_queue_call_mocked_callback(EVENT_VESC_LOCKED_CHANGED, &d);
    assert_int_equal(board_mode_get(), BOARD_MODE_IDLE);
}

static const struct CMUnitTest board_mode_vp_tests[] = {
    cmocka_unit_test_setup(test_vp_boot_timer, board_mode_setup),
    cmocka_unit_test_setup(test_vp_hold_off_locked_abort, board_mode_setup),
    cmocka_unit_test_setup(test_vp_comm_loss_idle_faults_quietly_then_clears, board_mode_setup),
    cmocka_unit_test_setup(test_vp_comm_loss_riding_faults_then_clears, board_mode_setup),
    cmocka_unit_test_setup(test_vp_other_fault_still_latches, board_mode_setup),
};

int main(void)
{
    int result = 0;
    result += cmocka_run_group_tests_name("Board Mode Test", board_mode_tests, NULL, NULL);
    result += cmocka_run_group_tests_name("Board Mode VP Test", board_mode_vp_tests, NULL, NULL);
    return result;
}
