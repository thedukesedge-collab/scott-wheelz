/*
 * Copyright (c) 2024-2026, Mitchell White <mitchell.n.white@gmail.com>
 *
 * This file is part of Advanced LCM (ALCM) project.
 *
 * ALCM is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * ALCM is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along
 * with ALCM. If not, see <https://www.gnu.org/licenses/>.
 */
#include <string.h>

#include "vesc_serial.h"
#ifdef LCM_BOOTLOADER
#include "buzzer_hw.h"
#include "headlights_hw.h"
#include "hk32f030m.h"
#endif
#include "vesc_serial_hw.h"
#include "event_queue.h"
#include "timer.h"
#include "crc16_ccitt.h"
#include "lcm_types.h"
#include "interrupts.h"
#include "tiny_math.h"
#include "function_generator.h"
#if defined(ENABLE_VOLTAGE_MONITORING)
#include "battery_lut.h"
#endif
#ifdef ENABLE_APP_INTEGRATION
#include "settings.h"
#include "command_processor.h"
#endif

// Values from VESC datatypes.h
#define COMM_GET_VALUES_SETUP_SELECTIVE 51
#define COMM_GET_VALUES_SETUP_SELECTIVE_RESPONSE_LENGTH 20
#define COMM_GET_VALUES_SETUP_SELECTIVE_MASK 0x101b8

#ifdef ENABLE_IMU_EVENTS
#define COMM_GET_IMU_DATA 65
#define COMM_GET_IMU_DATA_RESPONSE_LENGTH 12
#define COMM_GET_IMU_DATA_MASK 0x03
#endif

#ifdef ENABLE_APP_INTEGRATION
/* refloat's custom app data protocol (COMM_CUSTOM_APP_DATA), used to relay
 * phone-app-controlled headlight/status bar brightness. See
 * refloat/src/lcm.h (LcmCommands) and refloat/src/lcm.c
 * (lcm_poll_response()) for the authoritative definition. */
#define COMM_CUSTOM_APP_DATA 36
#define LCM_PACKAGE_ID 101
#define LCM_COMMAND_POLL 24
#define LCM_POLL_RESPONSE_MIN_LENGTH 15
#define LCM_POLL_STATE_OFFSET 3
#define LCM_POLL_HEADLIGHT_BRIGHTNESS_OFFSET 12
#define LCM_POLL_STATUS_BRIGHTNESS_OFFSET 14
/* Sentinel baseline value (out of the valid 0-100 range) meaning "no poll
 * reply has been seen yet for this channel". */
#define LCM_BASELINE_UNSET 0xFFU
/* Low nibble of the state byte at LCM_POLL_STATE_OFFSET: refloat's
 * state_compat() (refloat/src/state.c) maps STATE_DISABLED to 0xF - this is
 * how the phone app's "Lock" feature is signalled over the wire. */
#define LCM_STATE_MASK 0x0FU
#define LCM_STATE_DISABLED 0x0FU

#ifdef VP_LIGHT_CONTROL
/* Refloat Glow (VP) light-control block, appended by the VP package
 * right after refloat's standard COMMAND_LCM_POLL reply (which ends at
 * offset 14). When present, the app is the only source of light settings:
 * every field is applied whenever it differs from the current setting, and
 * the two brightness bytes (offsets 12 and 14) are applied directly instead
 * of only on change from a remote baseline.
 *
 *   15  magic 0x56 ('V')
 *   16  version (1)
 *   17  flags: bit0 headlights on, bit1 status bar on, bit2 beeper on,
 *       bit3 toggles once per test-beep request from the app,
 *       bit4 = update staged (bootloader builds: start the bootloader)
 *       bit5 = headlights dark in snooze
 *       bit6 = power off requested from the app (SLCM r22+; only taken
 *       while idle: the normal shutdown, as when holding the button)
 *   18  idle animation      (animation_option_t)
 *   19  riding animation
 *   20  dozing ("snooze") animation
 *   21  boot animation
 *   22  shutdown animation
 *   23  personal color, degrees, high byte
 *   24  personal color, low byte (0-359)
 *   25  battery % from the package's battery curve (0-100), 0xFF = none:
 *       use the VESC's (optional, newer packages)
 *
 *   26-33 CUSTOM animation parameters (package rf71+, to SLCM r19+ only)
 *
 * The whole reply is 25 (26) bytes, 33 (34) with the CUSTOM block, inside
 * MAX_PACKET_LENGTH (40; 32 before r19). */
#define VP_BLOCK_OFFSET 15U
#define VP_BLOCK_MAGIC 0x56U
#define VP_BLOCK_VERSION 1U
#define VP_REPLY_MIN_LENGTH 25U
#define VP_BATTERY_OFFSET 25U
#define VP_CUSTOM_OFFSET 26U // 8 bytes: CUSTOM animation (reply 34 bytes)
#define VP_FLAG_HEADLIGHTS 0x01U
#define VP_FLAG_STATUS_LEDS 0x02U
#define VP_FLAG_BEEP 0x04U
#define VP_FLAG_TEST_BEEP 0x08U // toggled by the app for a test beep
#define VP_FLAG_UPDATE 0x10U    // firmware update staged: hand over to the bootloader
#define VP_FLAG_DOZE_DARK 0x20U // headlights off in snooze (no breathing)
#define VP_FLAG_POWER_OFF 0x40U // app asks for a shutdown (button-hold equivalent)
#endif
#endif

#define SERIAL_BAUDRATE 115200U

/* The VESC serial messages are typically < 80 bytes, but 128 gives
 * a good margin. */
#define VESC_SERIAL_RX_BUFFER_SIZE 128U
#define POLLING_INTERVAL_MS 250U

#define START_BYTE 0x02
#define END_BYTE 0x03
#define MAX_PACKET_LENGTH 40 // VP reply with the CUSTOM block: 34
#define MAX_OUTSTANDING_PACKETS 5
// Millidegrees - 0.02 degrees was the original float threshold.
#define SIGNIFICANT_CHANGE(x, y) ((((x) > (y)) ? ((x) - (y)) : ((y) - (x))) > 20)

typedef struct
{
#if defined(ENABLE_VOLTAGE_MONITORING)
    int32_t current_centiamps; // Hundredths of an amp, positive while discharging
#endif
    int16_t duty_cycle;  // Tenths of a percent
    int32_t rpm;
#if defined(ENABLE_VOLTAGE_MONITORING)
    int16_t input_voltage; // Tenths of a volt
#endif
    int16_t battery_level; // Tenths of a percent
    uint8_t fault;
} comm_get_values_setup_selective_t;

#ifdef ENABLE_IMU_EVENTS
typedef struct
{
    int32_t pitch; // Millidegrees
    int32_t roll;  // Millidegrees
} comm_get_imu_data_t;
#endif

static volatile ring_buffer_t vesc_serial_rx_buffer = {0};
static volatile uint8_t vesc_serial_rx_buffer_data[VESC_SERIAL_RX_BUFFER_SIZE] = {0};
static timer_id_t vesc_serial_tx_timerid = INVALID_TIMER_ID;
static comm_get_values_setup_selective_t comm_get_values_setup_selective = {0};
static bool_t vesc_alive = false;
static uint8_t vesc_serial_outstaning_packet_count = 0;
static vesc_serial_callback_t vesc_serial_callback = NULL;
#ifdef ENABLE_IMU_EVENTS
static comm_get_imu_data_t comm_get_imu_data = {0};
#endif
#if defined(ENABLE_VOLTAGE_MONITORING)
// Shift-based EMA (no division/float) smoothing the voltage fed to the
// battery LUT lookup - without this, a locally-computed battery_level can
// be steppier than the VESC's own filtered value, and
// EVENT_BATTERY_LEVEL_CHANGED has no debounce (status_leds.c re-triggers
// its animation/refresh on every firing).
static int16_t vesc_serial_filtered_voltage = 0;
static bool_t vesc_serial_filtered_voltage_init = false;
#endif
#ifdef ENABLE_APP_INTEGRATION
#ifndef VP_LIGHT_CONTROL
static uint8_t headlight_remote_baseline = LCM_BASELINE_UNSET;
static uint8_t status_remote_baseline = LCM_BASELINE_UNSET;
#else
// Stock Refloat (no VP block): Refloat's brightness drives the lights from
// the first poll. A click-click level (1-3, 0 = follow Refloat) overrides it
// until Refloat's values change. Levels: headlights 0/60/100 %, status bar
// 30/60/100 %.
#define LCM_POLL_HEADLIGHT_IDLE_BRIGHTNESS_OFFSET 13
static const uint8_t stock_level_pct[2][4] = {{0U, 0U, 60U, 100U}, {0U, 30U, 60U, 100U}};
static uint8_t stock_level = 0U;
static uint32_t stock_key = 0xFFFFFFFFU; // Refloat's last 3 brightness bytes
#endif
#ifdef VP_LIGHT_CONTROL
static uint8_t vp_test_beep_last = 0xFFU; // 0xFF: no VP block seen yet
// app changes are written to the LCM's EEPROM this many polls (250 ms)
// later, idle only - so they also apply with the package off, power cut or not
#define VP_SAVE_POLLS 12U
static uint8_t vp_save_countdown = 0U;
static int16_t vp_battery_level = -1;      // tenths of a %, -1: the VESC's
#endif
static bool_t vesc_locked = false;
#endif

// Forward declarations
EVENT_HANDLER(vesc_serial, rx);
EVENT_HANDLER(vesc_serial, board_mode_change);
TIMER_CALLBACK(vesc_serial, tx);

/**
 * @brief Initializes the VESC serial module
 *
 * Initializes the VESC serial module by initializing the ring buffer,
 * subscribing to the VESC serial data event, and initializing the
 * comm_get_values_setup_selective struct.
 */
lcm_status_t vesc_serial_init(void)
{
    lcm_status_t status = LCM_SUCCESS;

    // Initialize the ring buffer
    vesc_serial_rx_buffer.buffer = vesc_serial_rx_buffer_data;
    vesc_serial_rx_buffer.size = VESC_SERIAL_RX_BUFFER_SIZE;
    vesc_serial_rx_buffer.read_idx = 0U;
    vesc_serial_rx_buffer.write_idx = 0U;

    // Initialize local data structures 
    memset(&comm_get_values_setup_selective, 0, sizeof(comm_get_values_setup_selective));
#ifdef ENABLE_IMU_EVENTS
    memset(&comm_get_imu_data, 0, sizeof(comm_get_imu_data));
#endif

    // Assume VESC is not alive
    vesc_alive = false;
#ifdef ENABLE_APP_INTEGRATION
#ifndef VP_LIGHT_CONTROL
    headlight_remote_baseline = LCM_BASELINE_UNSET;
    status_remote_baseline = LCM_BASELINE_UNSET;
#else
    stock_level = 0U;
    stock_key = 0xFFFFFFFFU;
#endif
#ifdef VP_LIGHT_CONTROL
    vp_test_beep_last = 0xFFU;
    vp_battery_level = -1;
#endif
    vesc_locked = false;
#endif

    vesc_serial_hw_init(SERIAL_BAUDRATE);

#if defined(ENABLE_VOLTAGE_MONITORING)
    // Validate a patched battery LUT once at boot; falls back to the
    // VESC's own battery_level automatically if none is present/valid.
    battery_lut_init();
    vesc_serial_filtered_voltage_init = false;
#endif

    // Subscribe to the VESC serial data event
    SUBSCRIBE_EVENT(vesc_serial, EVENT_SERIAL_DATA_RX, rx);
    SUBSCRIBE_EVENT(vesc_serial, EVENT_BOARD_MODE_CHANGED, board_mode_change);

    return status;
}

/**
 * @brief Clears the outstanding packets count and calls the callback if set
 *
 * This function is called when the VESC serial module is no longer busy.
 * It clears the outstanding packet count and calls the callback if it is set.
 */
void clear_outstanding_packets(void)
{
    // If someone is waiting for a callback, call it now
    if (vesc_serial_callback != NULL)
    {
        vesc_serial_callback();
        vesc_serial_callback = NULL;
    }
    vesc_serial_outstaning_packet_count = 0;
}

/**
 * @brief Checks if the VESC serial is busy and sets a callback if it is
 * busy.
 */
lcm_status_t vesc_serial_check_busy_and_set_callback(vesc_serial_callback_t callback)
{
    lcm_status_t status = LCM_SUCCESS;

    // If the VESC is alive and we are busy, set the callback
    if ((vesc_alive == true) && (vesc_serial_outstaning_packet_count > 0U))
    {
        status = LCM_BUSY;
        vesc_serial_callback = callback;
    }

    return status;
}

/**
 * @brief Gets the VESC serial RX buffer
 *
 * Returns a pointer to the RX buffer ring buffer.
 *
 * @return A pointer to the RX buffer ring buffer.
 */
ring_buffer_t *vesc_serial_get_rx_buffer(void)
{
    return (ring_buffer_t *)&vesc_serial_rx_buffer;
}

/**
 * @brief Extracts a 16-bit signed integer from a buffer
 *
 * Extracts a 16-bit signed integer from the buffer, where the first byte is the
 * most significant and the second byte is the least significant.
 *
 * @param buffer The buffer to read from
 * @return The extracted integer
 */
int16_t buffer_get_int16(const uint8_t *buffer)
{
    return (int16_t)((buffer[0] << 8) | buffer[1]);
}

/**
 * @brief Extracts an unsigned 16-bit integer from a buffer
 *
 * Extracts an unsigned 16-bit integer from the buffer, where the first byte is the
 * most significant and the second byte is the least significant.
 *
 * @param buffer The buffer to read from
 * @return The extracted integer
 */
uint16_t buffer_get_uint16(const uint8_t *buffer)
{
    return (uint16_t)((buffer[0] << 8) | buffer[1]);
}

/**
 * @brief Extracts a 32-bit signed integer from a buffer
 *
 * Extracts a 32-bit signed integer from the buffer, where the first byte is the
 * most significant and the last byte is the least significant.
 *
 * @param buffer The buffer to read from
 * @return The extracted integer
 */
int32_t buffer_get_int32(const uint8_t *buffer)
{
    return (int32_t)((uint32_t)buffer[0] << 24 |
                     (uint32_t)buffer[1] << 16 |
                     (uint32_t)buffer[2] << 8  |
                     (uint32_t)buffer[3]);
}

/**
 * @brief Extracts a 32-bit unsigned integer from a buffer
 *
 * Extracts a 32-bit unsigned integer from the buffer, where the first byte is the
 * most significant and the last byte is the least significant.
 *
 * @param buffer The buffer to read from
 * @return The extracted integer
 */
uint32_t buffer_get_uint32(const uint8_t *buffer)
{
    return (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3];
}

#ifdef ENABLE_IMU_EVENTS
// 180000/pi (~57295.78), rounded to a plain integer - deliberately NOT
// wrapped in FIXED16(). radians_fixed16 below is already Q16.16 (pre-scaled
// by 65536), so this constant only needs to carry the real degrees-per-
// radian multiplier; the single ">>16" after multiplying by it undoes
// radians_fixed16's own scaling and leaves a plain millidegree integer.
// Wrapping it in FIXED16() (i.e. multiplying by another 65536) would try to
// fit ~57295.78*65536 (~3.75 billion) into an int32_t - that overflow was a
// real bug here: it silently wrapped to a large negative constant, so any
// nonzero pitch/roll decoded as thousands of degrees and instantly tripped
// any +-60 degree threshold check.
#define RADIANS_TO_MILLIDEGREES_SCALE ((int32_t)(180000.0 / 3.14159265358979323846 + 0.5))

/**
 * @brief Decodes a raw wire IEEE754 float (radians) directly into
 * millidegrees, via manual bit unpacking - no soft-float ops.
 *
 * This is the one place VESC data arrives as a genuine IEEE754 bit pattern
 * (everywhere else uses buffer_get_int16/32, which are already integers).
 * Denormals/true-zero (raw exponent field 0) are treated as 0 - negligible
 * for a physically meaningful pitch/roll reading. Values whose magnitude
 * would overflow the Q16.16 intermediate are clamped rather than wrapped.
 *
 * @param bits Raw 32-bit wire value (big-endian decoded via buffer_get_uint32).
 * @return The angle in millidegrees.
 */
static int32_t buffer_get_imu_millidegrees(const uint8_t *buffer)
{
    uint32_t bits = buffer_get_uint32(buffer);
    bool_t negative = (bits & 0x80000000U) != 0U;
    int32_t exponent = (int32_t)((bits >> 23) & 0xFFU);
    uint32_t mantissa;
    int32_t shift;
    int64_t radians_fixed16;
    int64_t result;

    if (exponent == 0)
    {
        return 0;
    }

    mantissa = (bits & 0x007FFFFFU) | 0x00800000U; // restore implicit leading 1
    shift = (exponent - 127) - 7; // scale 23-bit mantissa fraction to Q16.16

    if (shift >= 0)
    {
        radians_fixed16 = (shift > 30) ? INT32_MAX : ((int64_t)mantissa << shift);
    }
    else
    {
        radians_fixed16 = (shift < -30) ? 0 : ((int64_t)mantissa >> (-shift));
    }
    if (radians_fixed16 > INT32_MAX)
    {
        radians_fixed16 = INT32_MAX;
    }

    result = ((int64_t)(int32_t)radians_fixed16 * RADIANS_TO_MILLIDEGREES_SCALE) >> 16;

    return (int32_t)(negative ? -result : result);
}

/**
 * @brief Processes a COMM_GET_IMU_DATA packet
 *
 * This function is an event handler for the EVENT_SERIAL_DATA_RX event.
 * It copies the payload into a temporary struct and then checks if each field
 * has changed. If a field has changed, it pushes an event to the event queue
 * and updates the comm_get_imu_data struct.
 * @param payload The payload of the packet
 * @param packet_length The length of the packet
 */
void process_comm_get_imu_data(const uint8_t *payload, uint8_t packet_length)
{
    comm_get_imu_data_t imu_data = {0};
    uint16_t mask = 0;

    // Expect a specific packet length for the fields selected.
    // If the packet length is incorrect, the only thing we can do is abort,
    // since we can't make any assumptions about the contents of the packet.
    if (packet_length != COMM_GET_IMU_DATA_RESPONSE_LENGTH)
    {
        fault(EMERGENCY_FAULT_INVALID_LENGTH);
        return;
    }

    // Response contains a 16-bit mask of the fields we requested
    mask = buffer_get_uint16(&payload[1]);
    if (mask != COMM_GET_IMU_DATA_MASK)
    {
        // Invalid mask
        fault(EMERGENCY_FAULT_OUT_OF_BOUNDS);
        return;
    }

    // Copy the payload into the temporary comm_get_imu_data struct - both
    // already in millidegrees, so cache and fresh value are always
    // comparable (the previous float version cached the fresh sample's
    // degrees value but compared it against the next sample's radians
    // value - a latent unit mismatch this rewrite avoids for free).
    imu_data.roll = buffer_get_imu_millidegrees(&payload[3]);
    imu_data.pitch = buffer_get_imu_millidegrees(&payload[7]);

    // For each field, check if the value has changed
    if (SIGNIFICANT_CHANGE(imu_data.pitch, comm_get_imu_data.pitch))
    {
        event_data_t data = {0};
        data.imu_pitch = imu_data.pitch;
        event_queue_push(EVENT_IMU_PITCH_CHANGED, &data);

        comm_get_imu_data.pitch = imu_data.pitch;
    }

    if (SIGNIFICANT_CHANGE(imu_data.roll, comm_get_imu_data.roll))
    {
        event_data_t data = {0};
        data.imu_roll = imu_data.roll;
        event_queue_push(EVENT_IMU_ROLL_CHANGED, &data);

        comm_get_imu_data.roll = imu_data.roll;
    }
}
#endif

/**
 * @brief Processes a COMM_GET_VALUES_SETUP_SELECTIVE packet
 *
 * This function is an event handler for the EVENT_SERIAL_DATA_RX event.
 * It copies the payload into a temporary struct and then checks if each field
 * has changed. If a field has changed, it pushes an event to the event queue
 * and updates the comm_get_values_setup_selective struct.
 * @param payload The payload of the packet
 * @param packet_length The length of the packet
 */
void process_comm_get_values_setup_selective(const uint8_t *payload, uint8_t packet_length)
{
    comm_get_values_setup_selective_t values = {0};
    uint32_t values_mask = 0;

    // Expect a specific packet length for the fields selected.
    // If the packet length is incorrect, the only thing we can do is abort,
    // since we can't make any assumptions about the contents of the packet.
    if (packet_length != COMM_GET_VALUES_SETUP_SELECTIVE_RESPONSE_LENGTH)
    {
        fault(EMERGENCY_FAULT_INVALID_LENGTH);
        return;
    }

    // Response contains a 32-bit mask of the fields we requested
    values_mask = buffer_get_uint32(&payload[1]);
    if (values_mask != COMM_GET_VALUES_SETUP_SELECTIVE_MASK)
    {
        // Invalid mask
        fault(EMERGENCY_FAULT_OUT_OF_BOUNDS);
        return;
    }

#if defined(ENABLE_VOLTAGE_MONITORING)
    // Battery current, needed for IR-drop compensation below. Decode and
    // clamp first since the compensated voltage depends on it.
    values.current_centiamps = buffer_get_int32(&payload[5]);
    values.current_centiamps = CLAMP(values.current_centiamps, -20000, 20000); // +-200A
#endif

    // Copy the payload into the temporary comm_get_values_setup_selective
    // struct. The wire value is already tenths of a percent (an int16,
    // previously divided by 10.0f into a float and multiplied back out
    // downstream) - decode it directly.
    values.duty_cycle = buffer_get_int16(&payload[9]);

    // Coerce the duty cycle to a valid range (previously a no-op bug: CLAMP
    // used as a bare statement discarded its result instead of assigning it).
    values.duty_cycle = CLAMP(values.duty_cycle, -1000, 1000);

    // RPM is a genuine integer quantity in the VESC protocol (scale 1.0) -
    // decode it directly instead of round-tripping through
    // buffer_get_float32(), which only ever divided by 1.0f.
    values.rpm = buffer_get_int32(&payload[11]);

#if defined(ENABLE_VOLTAGE_MONITORING)
    values.input_voltage = buffer_get_int16(&payload[15]);

    // The VESC can report voltage spikes/dropouts outside a sane range;
    // coerce before it feeds the compensation/EMA/LUT below (previously
    // unclamped).
    values.input_voltage = CLAMP(values.input_voltage, 0, 2000);

    // A valid LUT block also carries an (optional, may be 0/disabled)
    // r_int_milliohms - compensate the terminal voltage back toward
    // open-circuit before it's smoothed/looked up, so a hill climb's
    // current draw doesn't read as a big SoC drop that "heals" the
    // instant the load lets up.
    int16_t voltage_for_lut = values.input_voltage;
    if (battery_lut_is_valid())
    {
        voltage_for_lut = (int16_t)(values.input_voltage +
                                     battery_lut_get_voltage_compensation(values.current_centiamps));
    }

    if (!vesc_serial_filtered_voltage_init)
    {
        vesc_serial_filtered_voltage = voltage_for_lut;
        vesc_serial_filtered_voltage_init = true;
    }
    else
    {
        vesc_serial_filtered_voltage +=
            (voltage_for_lut - vesc_serial_filtered_voltage) >> 2;
    }
#endif

    values.battery_level = buffer_get_int16(&payload[17]);

    // The VESC can return battery levels outside of the 0-100% range,
    // so we need to coerce it to a valid range.
    values.battery_level = CLAMP(values.battery_level, 0, 1000);

#if defined(ENABLE_VOLTAGE_MONITORING)
    // A patched, valid LUT block overrides the VESC's own calculation with
    // one calibrated to the rider's actual pack chemistry. Unpatched (the
    // default) or invalid falls straight through to the VESC's value
    // above, unchanged.
    if (battery_lut_is_valid())
    {
        values.battery_level = battery_lut_get_percent((uint16_t)vesc_serial_filtered_voltage);
    }
#endif

    values.fault = payload[19];

    // For each field, check if the value has changed
    if (values.duty_cycle != comm_get_values_setup_selective.duty_cycle)
    {
        event_data_t data = {0};
        data.duty_cycle = values.duty_cycle;
        event_queue_push(EVENT_DUTY_CYCLE_CHANGED, &data);

        comm_get_values_setup_selective.duty_cycle = values.duty_cycle;
    }

    if (values.rpm != comm_get_values_setup_selective.rpm)
    {
        event_data_t data = {0};
        data.rpm = values.rpm;
        event_queue_push(EVENT_RPM_CHANGED, &data);

        comm_get_values_setup_selective.rpm = values.rpm;
    }

#if defined(ENABLE_VOLTAGE_MONITORING)
    if (values.input_voltage != comm_get_values_setup_selective.input_voltage)
    {
        event_data_t data = {0};
        data.voltage = values.input_voltage;
        event_queue_push(EVENT_VOLTAGE_CHANGED, &data);

        comm_get_values_setup_selective.input_voltage = values.input_voltage;
    }
#endif

#ifdef VP_LIGHT_CONTROL
    // the package's battery curve, when it sends one, wins
    if (vp_battery_level >= 0)
    {
        values.battery_level = vp_battery_level;
    }
#endif

    if (values.battery_level != comm_get_values_setup_selective.battery_level)
    {
        event_data_t data = {0};
        data.battery_level = values.battery_level;
        event_queue_push(EVENT_BATTERY_LEVEL_CHANGED, &data);

        comm_get_values_setup_selective.battery_level = values.battery_level;
    }

    /*
     * Handle fault changes.
     */
    if (values.fault != comm_get_values_setup_selective.fault)
    {
        event_data_t data = {0};
        data.vesc_fault = values.fault;
        event_queue_push(EVENT_VESC_FAULT_CHANGED, &data);
        comm_get_values_setup_selective.fault = values.fault;
    }
}

#if defined(ENABLE_APP_INTEGRATION) && !defined(VP_LIGHT_CONTROL)
/**
 * @brief Applies a single refloat-reported brightness channel if it changed
 *
 * Shared by both the headlight and status bar channels of
 * process_comm_custom_app_data(). A channel's baseline starts at
 * LCM_BASELINE_UNSET (out of the valid 0-100 range); the first sample just
 * records the baseline without touching the setting, so a poll reply that
 * merely confirms refloat's already-applied value never re-fires an event.
 * Only a genuine change from the last-seen remote value is applied, which is
 * what lets local button/footpad adjustments stick between polls instead of
 * being overwritten by refloat re-reporting the same value it always has.
 *
 * @param baseline Last remote value seen for this channel (persists across calls)
 * @param new_pct New value from the wire, 0-100 (clamped defensively)
 * @param setting Settings field to update (0-255 = 0.0-1.0 fraction)
 * @param context Context to report on EVENT_COMMAND_SETTINGS_CHANGED
 */
static void apply_lcm_brightness(uint8_t *baseline, uint8_t new_pct, uint8_t *setting,
                                  command_processor_context_t context)
{
    if (new_pct > 100U)
    {
        new_pct = 100U;
    }

    if (new_pct != *baseline)
    {
        bool_t first_sample = (*baseline > 100U);
        *baseline = new_pct;

        if (!first_sample)
        {
            event_data_t data = {0};

            // Divisor is a compile-time constant (100), so this is a free
            // multiply-by-reciprocal, not a runtime division call.
            *setting = (uint8_t)(((uint16_t)new_pct * 255U) / 100U);
            data.context = context;
            event_queue_push(EVENT_COMMAND_SETTINGS_CHANGED, &data);
        }
    }
}
#endif

#ifdef ENABLE_APP_INTEGRATION

#ifdef VP_LIGHT_CONTROL
/**
 * @brief Sets a brightness setting from a 0-100 % value if it differs
 */
static void vp_apply_brightness(uint8_t pct, uint8_t *setting, command_processor_context_t context)
{
    uint8_t value;

    if (pct > 100U)
    {
        pct = 100U;
    }
    value = (uint8_t)(((uint16_t)pct * 255U) / 100U);

    if (value != *setting)
    {
        event_data_t data = {0};

        *setting = value;
        data.context = context;
        event_queue_push(EVENT_COMMAND_SETTINGS_CHANGED, &data);
    }
}

/**
 * @brief Sets an animation setting if the value is valid and differs
 *
 * @return true if the setting changed
 */
static bool_t vp_apply_animation(animation_option_t *setting, uint8_t value)
{
    bool_t changed = false;

    if ((value < (uint8_t)ANIMATION_OPTION_COUNT) && ((uint8_t)*setting != value))
    {
        *setting = (animation_option_t)value;
        changed = true;
    }

    return changed;
}

/**
 * @brief Applies the VP light-control block (see VP_BLOCK_OFFSET)
 *
 * On/off flags and animation changes are followed by one
 * EVENT_COMMAND_TOGGLE_LIGHTS, which makes the headlights and status bar
 * re-apply their enable setting and restart the display for the current
 * board mode (so a new idle animation shows up right away when idle, and a
 * new riding animation only once riding). Settings are saved to EEPROM by
 * the settings module at shutdown, as before.
 *
 * @param block Pointer to the block (payload + VP_BLOCK_OFFSET)
 * @param settings Current settings
 */
#ifdef LCM_BOOTLOADER
/**
 * @brief Hands over to the LCM bootloader for a firmware update
 *
 * The bootloader keeps its update entry in vector slot 7 (0x0800001C, a
 * reserved slot on Cortex-M0). It holds the power latch (PA2) on, pulls the
 * image from the package, and starts the new app - or this one again if
 * nothing comes. Lights and buzzer are switched off first; interrupts are
 * masked and never come back on here.
 */
static void vp_enter_bootloader(void)
{
    uint32_t entry = *(volatile const uint32_t *)0x0800001CU;

    if (((entry & 1U) == 0U) || (entry < 0x08000000U) || (entry >= 0x08000600U))
    {
        return; // no bootloader on this LCM
    }
    __disable_irq();
    headlights_hw_set_brightness(0U);
    buzzer_off();
    // Stop our 1 ms tick and drop a tick that may already be pending: the
    // bootloader's vector table has no SysTick slot (it also clears the
    // pending bit before handing back, this is the first line of defence)
    SysTick->CTRL = 0U;
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk;
    ((void (*)(void))entry)();
}
#endif

static void vp_apply_block(const uint8_t *block, settings_t *settings)
{
    bool_t refresh = false;
    uint8_t flags = block[2];
    bool_t headlights = ((flags & VP_FLAG_HEADLIGHTS) != 0U) ? true : false;
    bool_t status_leds = ((flags & VP_FLAG_STATUS_LEDS) != 0U) ? true : false;
    bool_t beep = ((flags & VP_FLAG_BEEP) != 0U) ? true : false;
    uint16_t color = (uint16_t)(((uint16_t)block[8] << 8) | block[9]);

    if ((flags & VP_FLAG_DOZE_DARK) != settings->doze_dark) // from the next snooze
    {
        settings->doze_dark = flags & VP_FLAG_DOZE_DARK;
        vp_save_countdown = VP_SAVE_POLLS;
    }

    if ((headlights != settings->enable_headlights) ||
        (status_leds != settings->enable_status_leds))
    {
        settings->enable_headlights = headlights;
        settings->enable_status_leds = status_leds;
        refresh = true;
    }

    if (beep != settings->enable_beep)
    {
        vp_save_countdown = VP_SAVE_POLLS;
        settings->enable_beep = beep;
        event_queue_push(EVENT_COMMAND_TOGGLE_BEEPER, NULL);
    }

    if (vp_apply_animation(&settings->idle_animation, block[3]))
    {
        refresh = true;
    }
    if (vp_apply_animation(&settings->ride_animation, block[4]))
    {
        refresh = true;
    }
    if (vp_apply_animation(&settings->dozing_animation, block[5]))
    {
        refresh = true;
    }
    if (vp_apply_animation(&settings->boot_animation, block[6]))
    {
        refresh = true;
    }
    if (vp_apply_animation(&settings->shutdown_animation, block[7]))
    {
        refresh = true;
    }

    if ((color < 360U) && (color != settings->personal_color))
    {
        event_data_t data = {0};

        settings->personal_color = color;
        data.context = COMMAND_PROCESSOR_CONTEXT_PERSONAL_COLOR;
        event_queue_push(EVENT_COMMAND_SETTINGS_CHANGED, &data);
        refresh = true; // back to the normal display after the color update
    }

    if (refresh)
    {
        vp_save_countdown = VP_SAVE_POLLS;
        event_queue_push(EVENT_COMMAND_TOGGLE_LIGHTS, NULL);
    }

#ifdef LCM_BOOTLOADER
    // Update staged (the package only sets this while the board isn't
    // running); only taken while idle - never while riding or booting
    if (((flags & VP_FLAG_UPDATE) != 0U) && (board_mode_get() == BOARD_MODE_IDLE))
    {
        vp_enter_bootloader();
    }
#endif

    // Power off from the app (for a board whose button can't be read): the
    // same shutdown as holding the button, only while idle
    if (((flags & VP_FLAG_POWER_OFF) != 0U) && (board_mode_get() == BOARD_MODE_IDLE))
    {
        event_queue_push(EVENT_COMMAND_SHUTDOWN, NULL);
    }

    // Test beep: the app toggles bit 3; the first block only records it
    flags &= VP_FLAG_TEST_BEEP;
    if (flags != vp_test_beep_last)
    {
        if (vp_test_beep_last != 0xFFU)
        {
            event_queue_push(EVENT_COMMAND_ACK, NULL);
        }
        vp_test_beep_last = flags;
    }
}
#endif

/**
 * @brief Processes a COMM_CUSTOM_APP_DATA packet
 *
 * Looks for refloat's COMMAND_LCM_POLL response and relays any change in the
 * reported headlight/status bar brightness into the local settings. A
 * response shorter than expected, or one that doesn't match the expected
 * package/command id, is a valid protocol state (e.g. refloat's external LED
 * support is disabled) rather than an error, so it is silently ignored.
 *
 * @param payload The payload of the packet
 * @param packet_length The length of the packet
 */
void process_comm_custom_app_data(const uint8_t *payload, uint8_t packet_length)
{
    settings_t *settings;

    if (packet_length < LCM_POLL_RESPONSE_MIN_LENGTH || payload[1] != LCM_PACKAGE_ID ||
        payload[2] != LCM_COMMAND_POLL)
    {
        return;
    }

    settings = settings_get();

    {
        bool_t new_locked = (payload[LCM_POLL_STATE_OFFSET] & LCM_STATE_MASK) == LCM_STATE_DISABLED;

        if (new_locked != vesc_locked)
        {
            event_data_t lock_data = {0};

            vesc_locked = new_locked;
            lock_data.enable = new_locked;
            event_queue_push(EVENT_VESC_LOCKED_CHANGED, &lock_data);
        }
    }

#ifdef VP_LIGHT_CONTROL
    if ((packet_length >= VP_REPLY_MIN_LENGTH) &&
        (payload[VP_BLOCK_OFFSET] == VP_BLOCK_MAGIC) &&
        (payload[VP_BLOCK_OFFSET + 1U] == VP_BLOCK_VERSION))
    {
        vp_apply_brightness(payload[LCM_POLL_HEADLIGHT_BRIGHTNESS_OFFSET],
                            &settings->headlight_brightness,
                            COMMAND_PROCESSOR_CONTEXT_HEADLIGHT_BRIGHTNESS);
        vp_apply_brightness(payload[LCM_POLL_STATUS_BRIGHTNESS_OFFSET],
                            &settings->status_brightness,
                            COMMAND_PROCESSOR_CONTEXT_STATUS_BAR_BRIGHTNESS);
        vp_apply_block(&payload[VP_BLOCK_OFFSET], settings);

        // CUSTOM animation parameters (package rf71+)
        if ((packet_length >= VP_CUSTOM_OFFSET + sizeof(settings->custom_anim)) &&
            (memcmp(settings->custom_anim, &payload[VP_CUSTOM_OFFSET],
                    sizeof(settings->custom_anim)) != 0))
        {
            memcpy(settings->custom_anim, &payload[VP_CUSTOM_OFFSET],
                   sizeof(settings->custom_anim));
            vp_save_countdown = VP_SAVE_POLLS;
            event_queue_push(EVENT_COMMAND_TOGGLE_LIGHTS, NULL);
        }

        // battery % from the package's curve (takes effect with the next
        // values reply, at most 250 ms later)
        vp_battery_level = ((packet_length > VP_BATTERY_OFFSET) &&
                            (payload[VP_BATTERY_OFFSET] <= 100U))
                               ? (int16_t)(payload[VP_BATTERY_OFFSET] * 10U)
                               : -1;
        return;
    }
    vp_battery_level = -1; // no VP block (any more): the VESC's battery %
    vp_test_beep_last = 0xFFU; // = no VP block (a later one records, no beep)
    {
        // Stock Refloat: its Lights On/Off and LED brightness drive the
        // SLCM - headlights at the riding level while riding, the idle level
        // otherwise, and the status bar. A click-click level overrides them
        // until Refloat sends different values. Lights are switched on here
        // (RAM only, never saved) so a "headlights off" saved from the app
        // can't keep them dark on stock Refloat.
        uint32_t key = payload[LCM_POLL_HEADLIGHT_BRIGHTNESS_OFFSET] |
                       ((uint32_t)payload[LCM_POLL_HEADLIGHT_IDLE_BRIGHTNESS_OFFSET] << 8) |
                       ((uint32_t)payload[LCM_POLL_STATUS_BRIGHTNESS_OFFSET] << 16);
        uint8_t head = payload[(board_mode_get() == BOARD_MODE_RIDING)
                                   ? LCM_POLL_HEADLIGHT_BRIGHTNESS_OFFSET
                                   : LCM_POLL_HEADLIGHT_IDLE_BRIGHTNESS_OFFSET];
        uint8_t status = payload[LCM_POLL_STATUS_BRIGHTNESS_OFFSET];

        if (key != stock_key)
        {
            stock_key = key;
            stock_level = 0U;
        }
        if (stock_level != 0U)
        {
            head = stock_level_pct[0][stock_level];
            status = stock_level_pct[1][stock_level];
        }
        if (!settings->enable_headlights || !settings->enable_status_leds)
        {
            settings->enable_headlights = true;
            settings->enable_status_leds = true;
            event_queue_push(EVENT_COMMAND_TOGGLE_LIGHTS, NULL);
        }
        vp_apply_brightness(head, &settings->headlight_brightness,
                            COMMAND_PROCESSOR_CONTEXT_HEADLIGHT_BRIGHTNESS);
        vp_apply_brightness(status, &settings->status_brightness,
                            COMMAND_PROCESSOR_CONTEXT_STATUS_BAR_BRIGHTNESS);
    }
#else
    apply_lcm_brightness(&headlight_remote_baseline, payload[LCM_POLL_HEADLIGHT_BRIGHTNESS_OFFSET],
                          &settings->headlight_brightness,
                          COMMAND_PROCESSOR_CONTEXT_HEADLIGHT_BRIGHTNESS);
    apply_lcm_brightness(&status_remote_baseline, payload[LCM_POLL_STATUS_BRIGHTNESS_OFFSET],
                          &settings->status_brightness,
                          COMMAND_PROCESSOR_CONTEXT_STATUS_BAR_BRIGHTNESS);
#endif
}

#ifdef VP_LIGHT_CONTROL
/**
 * @brief Click-click on stock Refloat: next light level (1, 2, 3, 1, ...)
 *
 * Only without the VP block (with it, the app sets the lights) and never
 * while riding. Takes effect with the next poll reply (<= 250 ms).
 *
 * @return true if the level changed
 */
bool_t vesc_serial_stock_level_step(void)
{
    // vp_test_beep_last is 0xFF only while no VP block arrives
    if ((vp_test_beep_last != 0xFFU) || (board_mode_get() == BOARD_MODE_RIDING))
    {
        return false;
    }
    stock_level = (stock_level >= 3U) ? 1U : (uint8_t)(stock_level + 1U);
    return true;
}
#endif
#endif

/**
 * @brief Processes a VESC packet
 *
 * @param payload The payload of the packet. The first byte of the payload is
 *                the command ID.
 * @param packet_length The length of the packet, including the command ID.
 */
void process_packet(uint8_t *payload, uint8_t packet_length)
{
    // The first time we recieve a valid packet from the VESC, we know it is
    // alive
    if (vesc_alive == false)
    {
        event_queue_push(EVENT_VESC_ALIVE, NULL);
        vesc_alive = true;
    }

    // First byte of payload is the command ID
    switch (payload[0])
    {
    case COMM_GET_VALUES_SETUP_SELECTIVE:
        process_comm_get_values_setup_selective(payload, packet_length);
        break;
#ifdef ENABLE_IMU_EVENTS
    case COMM_GET_IMU_DATA:
        process_comm_get_imu_data(payload, packet_length);
        break;
#endif
#ifdef ENABLE_APP_INTEGRATION
    case COMM_CUSTOM_APP_DATA:
        process_comm_custom_app_data(payload, packet_length);
        break;
#endif
    default:
        // Unknown command
        break;
    }
}

/**
 * @brief Handles the reception of VESC serial data
 *
 * This event handler processes incoming serial data from the VESC. It
 * searches for the start byte in the data stream, extracts the packet
 * length, payload, CRC, and end byte. If a valid packet is found, with
 * the correct CRC and end byte, the packet is processed by calling
 * process_packet().
 */
EVENT_HANDLER(vesc_serial, rx)
{
    uint8_t byte = 0;
    uint16_t crc = 0;
    uint8_t packet_length = 0;

    // Initialization should not be necessary, since we will immediately
    // overwrite the buffer, but MISRA requires it
    uint8_t payload[MAX_PACKET_LENGTH] = {0};

    // Reset the outstanding packet count
    clear_outstanding_packets();

    // Search for start byte (or end of data)
    while (byte != START_BYTE && ring_buffer_pop((ring_buffer_t *)&vesc_serial_rx_buffer, &byte))
    {
        // If we found the start byte, read the rest of the packet
        if (byte == START_BYTE)
        {
            if (!ring_buffer_pop((ring_buffer_t *)&vesc_serial_rx_buffer, &packet_length) ||
                packet_length > MAX_PACKET_LENGTH)
            {
                return;
            }

            for (uint8_t i = 0; i < packet_length; i++)
            {
                if (!ring_buffer_pop((ring_buffer_t *)&vesc_serial_rx_buffer, &payload[i]))
                {
                    return;
                }
            }

            // Next two bytes should be the CRC
            if (!ring_buffer_pop((ring_buffer_t *)&vesc_serial_rx_buffer, &byte))
            {
                return;
            }
            crc = byte << 8;
            if (!ring_buffer_pop((ring_buffer_t *)&vesc_serial_rx_buffer, &byte))
            {
                return;
            }
            crc |= byte;

            // Last byte should be the end
            if (!ring_buffer_pop((ring_buffer_t *)&vesc_serial_rx_buffer, &byte))
            {
                return;
            }

            if (byte == END_BYTE)
            {
                // Check CRC
                if (crc16_ccitt(payload, packet_length) == crc)
                {
                    // Packet is valid
                    process_packet(payload, packet_length);
                }
            }
        }
    }
}

/**
 * @brief Handles board mode changes for VESC serial communication
 *
 * This function manages the VESC serial communication behavior
 * based on the current board mode. It sets a polling timer when
 * the board is in modes that require VESC data polling and cancels
 * the timer when the board is in modes that do not require polling.
 *
 * @param data The event data containing the current board mode
 */
EVENT_HANDLER(vesc_serial, board_mode_change)
{
    switch (data->board_mode.mode)
    {
    // modes where we want to poll the VESC
    case BOARD_MODE_BOOTING:
    case BOARD_MODE_IDLE:
    case BOARD_MODE_RIDING:
    case BOARD_MODE_FAULT:
    case BOARD_MODE_DISABLED:
        // Polling must continue while disabled (locked) - it's the only way
        // to ever learn the board has been unlocked again
        if (vesc_serial_tx_timerid == INVALID_TIMER_ID || !is_timer_active(vesc_serial_tx_timerid))
        {
            vesc_serial_tx_timerid =
                set_timer(POLLING_INTERVAL_MS, TIMER_CALLBACK_NAME(vesc_serial, tx), true);

            // Unlike most timers, losing VESC polling is safety-relevant -
            // it's the only way to detect the board being unlocked or
            // faulted, so this is the one caller that escalates a failed
            // allocation instead of silently tolerating INVALID_TIMER_ID.
            if (vesc_serial_tx_timerid == INVALID_TIMER_ID)
            {
                fault(EMERGENCY_FAULT_OVERFLOW);
            }
        }
        break;

    // modes where we don't want to poll the VESC
    default:
        vesc_alive = false;
        if (vesc_serial_tx_timerid != INVALID_TIMER_ID && is_timer_active(vesc_serial_tx_timerid))
        {
            cancel_timer(vesc_serial_tx_timerid);
            vesc_serial_tx_timerid = INVALID_TIMER_ID;
        }
        break;
    }
}

/**
 * @brief Timer callback for VESC serial communication
 *
 * This function is called by the timer subsystem when the VESC serial
 * communication timer expires. It sends a packet to the VESC to poll
 * for data. The packet is hardcoded so that the timer callback is
 * as lightweight as possible.
 */
TIMER_CALLBACK(vesc_serial, tx)
{
#ifdef VP_LIGHT_CONTROL
    // app changes to the light settings: written to the EEPROM a few seconds
    // later, never while riding (only if they differ, see settings_save())
    if ((vp_save_countdown != 0U) && (--vp_save_countdown == 0U))
    {
        if (board_mode_get() == BOARD_MODE_RIDING)
        {
            vp_save_countdown = 1U; // try again next poll
        }
        else
        {
            settings_save();
        }
    }
#endif

    /*
     * These packets are used to poll the VESC for data.  Since they are
     * sent repeatedly and never change, we can just hardcode each segment
     * to save time and reduce code complexity. The segments are
     * concatenated into a single UART write, one per optional feature.
     *
     * COMM_GET_VALUES_SETUP_SELECTIVE:
     * byte 0: start byte (0x02)
     * byte 1: packet length (0x05)
     * byte 2: command (0x33)
     * bytes 3-6: mask: 0x101b8  (u32)
     *   float 32: battery current, total (1<<3) - needed for IR-drop compensation
     *   float 16: duty cycle now (1<<4)
     *   int 32: RPM (1<<5)
     *   float 16: input voltage (1<<7)
     *   float 16: battery level (1<<8)
     *   int 8: fault (1<<16)
     * bytes 7-8 precomputed crc-16-ccitt (0xc0ee)
     * byte 9: end byte (0x03)
     *
     * Response payload field order follows the mask's bit order (current
     * before duty cycle, since bit 3 < bit 4) - see
     * COMM_GET_VALUES_SETUP_SELECTIVE_RESPONSE_LENGTH's parsing in
     * process_comm_get_values_setup_selective().
     *
     * COMM_GET_IMU_DATA (optional):
     * byte 0: start byte (0x02)
     * byte 1: packet length (0x03)
     * byte 2: command (0x41)
     * bytes 3-4: mask (0x0003) (u32)
     *  float 32: roll (1<<0)
     *  float 32: pitch (1<<1)
     * bytes 5-6 precomputed crc-16-ccitt (0x1afe)
     * byte 7: end byte (0x03)
     *
     * COMM_CUSTOM_APP_DATA / COMMAND_LCM_POLL (optional):
     * byte 0: start byte (0x02)
     * byte 1: packet length (0x03)
     * byte 2: command (0x24 = COMM_CUSTOM_APP_DATA, 36)
     * byte 3: refloat package id (0x65 = 101)
     * byte 4: refloat command id (0x18 = COMMAND_LCM_POLL, 24)
     * bytes 5-6 precomputed crc-16-ccitt (0x3de0)
     * byte 7: end byte (0x03)
     */
#if defined(ENABLE_IMU_EVENTS) && defined(ENABLE_APP_INTEGRATION)
    /* Refloat Glow: the GCC build passes in an LCM_POLL segment that
     * carries the firmware name ("VPBL-<build>" with the bootloader,
     * "VP-<build>" without), generated with its CRC by lcm/tools/lcm_tag.py.
     * Refloat stores it (lcm_poll_request) so the app can tell which LCM
     * firmware is fitted. Without it: the plain poll, as stock. */
#ifndef LCM_POLL_SEGMENT
#define LCM_POLL_SEGMENT 0x02, 0x03, 0x24, 0x65, 0x18, 0x3d, 0xe0, 0x03
#define LCM_POLL_SEGMENT_LENGTH 8U
#endif
#define BYTE_LENGTH (18U + LCM_POLL_SEGMENT_LENGTH)
    uint8_t buffer[BYTE_LENGTH] = {
        0x02, 0x05, 0x33, 0x00, 0x01, 0x01, 0xb8, 0xc0, 0xee, 0x03,
        0x02, 0x03, 0x41, 0x00, 0x03, 0x1a, 0xfe, 0x03,
        LCM_POLL_SEGMENT
    };
#elif defined(ENABLE_IMU_EVENTS)
#define BYTE_LENGTH 18U
    uint8_t buffer[BYTE_LENGTH] = {
        0x02, 0x05, 0x33, 0x00, 0x01, 0x01, 0xb8, 0xc0, 0xee, 0x03,
        0x02, 0x03, 0x41, 0x00, 0x03, 0x1a, 0xfe, 0x03
    };
#elif defined(ENABLE_APP_INTEGRATION)
#define BYTE_LENGTH 18U
    uint8_t buffer[BYTE_LENGTH] = {
        0x02, 0x05, 0x33, 0x00, 0x01, 0x01, 0xb8, 0xc0, 0xee, 0x03,
        0x02, 0x03, 0x24, 0x65, 0x18, 0x3d, 0xe0, 0x03
    };
#else
#define BYTE_LENGTH 10U
    uint8_t buffer[BYTE_LENGTH] = {
        0x02, 0x05, 0x33, 0x00, 0x01, 0x01, 0xb8, 0xc0, 0xee, 0x03
    };
#endif

    if (vesc_alive == true)
    {
        // If the VESC is alive, we keep track of the number of outstanding
        // (unanswered) packets.  If we exceed the maximum number of packets,
        // we trigger a fault and assume the VESC is dead.
        //
        // Note: the outstanding packet count is also used to determine if the
        // WS2812 LED updates should be disabled.
        if (vesc_serial_outstaning_packet_count++ >= MAX_OUTSTANDING_PACKETS)
        {
            // Too many outstanding packets, trigger a fault
            fault(EMERGENCY_FAULT_VESC_COMM_TIMEOUT);
            vesc_alive = false;
            clear_outstanding_packets();
        }
    }
    vesc_serial_hw_send(buffer, BYTE_LENGTH);
}

/**
 * @brief Returns the current duty cycle of the VESC
 *
 * @return The current duty cycle of the VESC
 */
int16_t vesc_serial_get_duty_cycle(void)
{
    return comm_get_values_setup_selective.duty_cycle;
}

/**
 * @brief Returns the current RPM of the VESC
 *
 * @return The current RPM of the VESC
 */
int32_t vesc_serial_get_rpm(void)
{
    return comm_get_values_setup_selective.rpm;
}

#if defined(ENABLE_VOLTAGE_MONITORING)
/**
 * @brief Returns the current input voltage of the VESC
 *
 * @return The current input voltage of the VESC
 */
int16_t vesc_serial_get_input_voltage(void)
{
    return comm_get_values_setup_selective.input_voltage;
}
#endif

/**
 * @brief Returns the current battery level of the VESC
 *
 * @return The current battery level of the VESC
 */
int16_t vesc_serial_get_battery_level(void)
{
    return comm_get_values_setup_selective.battery_level;
}

/**
 * @brief Returns the current fault of the VESC
 *
 * @return The current fault of the VESC
 */
uint8_t vesc_serial_get_fault(void)
{
    return comm_get_values_setup_selective.fault;
}

#ifdef ENABLE_IMU_EVENTS
/**
 * @brief Returns the current pitch of the VESC IMU in degrees
 *
 * @return The current pitch of the VESC IMU
 */
int32_t vesc_serial_get_imu_pitch(void)
{
    return comm_get_imu_data.pitch;
}

/**
 * @brief Returns the current roll of the VESC IMU in degrees
 *
 * @return The current roll of the VESC IMU
 */
int32_t vesc_serial_get_imu_roll(void)
{
    return comm_get_imu_data.roll;
}
#endif
