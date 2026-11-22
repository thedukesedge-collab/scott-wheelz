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

static const struct CMUnitTest board_mode_vp_tests[] = {
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
