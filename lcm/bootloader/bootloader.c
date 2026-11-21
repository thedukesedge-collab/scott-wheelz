// SPDX-License-Identifier: GPL-3.0-or-later
// Part of Refloat Glow (formerly Refloat rink-mixed), 2026. See FORK_NOTES.md.

/*
 * LCM bootloader for the HK32F030MF4P6 light control module
 * (Floatwheel ADV200 boards: XRV, GTV, PintV) running ALCM VP.
 *
 * Part of Re-Float Rink-Mixed. GPL-3.0-or-later.
 *
 * Flash map (16 KB, 128-byte pages):
 *   0x08000000 - 0x080005FF  this bootloader (1.5 KB, never written by it)
 *   0x08000600 - 0x08003F7F  ALCM application (vector table at 0x08000600,
 *                            selected with FLASH->INT_VEC_OFFSET = 0x600)
 *   0x08003F80 - 0x08003FFF  last page, holds the battery LUT block at
 *                            0x08003FC0 (tools/battery_lut_patch); never
 *                            written by updates, so the LUT is kept
 *
 * POWER: the LCM only stays powered while it drives PA2 (PWR_EN) high - the
 * button press powers it up, PA2 latches it. A chip reset would let go of
 * PA2, so nothing here ever resets the chip: we only jump. PA2 is driven
 * high before anything else in update mode and only released on purpose:
 * the button held 2 s. There is no timed power-off: we can't tell whether
 * someone is riding (the VESC balances without us), so power is never
 * dropped on a timer. The VP package refuses to engage while it hears us.
 *
 * HEADLIGHTS: while in update mode the headlights (white front) blink at
 * about 1 Hz while waiting and stay steady while pages arrive, so a rider
 * can tell the LCM is in its bootloader (the board won't start then).
 *
 * Normal power-up: if the application looks valid, jump to it at once with
 * nothing touched, so the app starts exactly as it did without us.
 *
 * Update: the app calls the entry point stored in vector slot 7
 * (0x0800001C) when the VP package asks for an update. We then pull the
 * image from the package over the existing VESC UART link, a page at a time:
 *
 *   LCM -> VESC  36 (COMM_CUSTOM_APP_DATA), 101, 128 (COMMAND_LCM_FW),
 *                0x10 (LCM_FW_REQ), offset (u32 BE)
 *   VESC -> LCM  36, 101, 128, 0x10, offset (u32), image size (u32),
 *                image CRC16 (u16), then 128 data bytes (the image is
 *                padded with 0xFF to whole 128-byte pages);
 *                size 0 = nothing staged / that page not there yet: ask
 *                again 20 ms later
 *   LCM -> VESC  36, 101, 128, 0x11 (LCM_FW_DONE), status:
 *                0 = written and verified, 1 = CRC mismatch (starts over),
 *                2 = nothing came, back to the unchanged app
 *
 * Fail-safe order:
 *  - page 0 (the app's vector table, which decides "is there an app") is
 *    erased before the first page is written and written LAST, back to
 *    front, so its stack pointer / reset vector words land last of all;
 *  - every page is read back and compared after writing;
 *  - the whole image CRC is checked before page 0 counts; on a mismatch
 *    page 0 is erased again, and after 3 bad images nothing more is
 *    written until the next power-up (no flash wear from a bad source).
 * So an update cut short at any point (power loss, phone gone) leaves no
 * valid app, and the next power-up comes straight back here, holds the
 * power on and waits for the image again - no ST-Link needed.
 *
 * The image CRC is CRC16-CCITT (XMODEM), the same as the VESC packet CRC.
 * No interrupts are used.
 */
#include "bl_hw.h"

#define APP_BASE 0x08000600u
#define FLASH_END 0x08003F80u /* last page = battery LUT, never written */
#define PAGE_SIZE 128u
#define MAX_IMAGE (FLASH_END - APP_BASE)

#define COMM_CUSTOM_APP_DATA 36u
#define LCM_PACKAGE_ID 101u
#define COMMAND_LCM_FW 128u
#define LCM_FW_REQ 0x10u
#define LCM_FW_DONE 0x11u

#define HDR 14u /* 36,101,128,sub, offset(4), size(4), crc(2) */
#define RX_MAX (HDR + PAGE_SIZE)

#define REQUEST_TIMEOUT_MS 300u
#define RETRY_MS 20u           /* page not there yet: ask again this much later */
#define GIVE_UP_MS 20000u      /* from the app, nothing staged: back to the app */
#define BUTTON_OFF_MS 2000u    /* button held 2 s (after a release): power off */
#define MAX_FAILS 3u           /* whole-image CRC failures before we stop writing */

/* ---------------------------------------------------------------- app */

int bl_app_valid(void)
{
    uint32_t sp = bl_rd32(APP_BASE);
    uint32_t pc = bl_rd32(APP_BASE + 4u);

    return ((sp - 0x20000001u) < 0x800u) && ((pc & 1u) != 0u) && (pc - APP_BASE < MAX_IMAGE);
}

/* ---------------------------------------------------------------- CRC */

static uint32_t crc16_step(uint32_t crc, uint32_t byte)
{
    crc ^= byte << 8;
    for (int i = 0; i < 8; i++)
    {
        crc <<= 1;
        if (crc & 0x10000u)
        {
            crc ^= 0x1021u;
        }
    }
    return crc & 0xFFFFu;
}

static uint32_t crc16_buf(const uint8_t *p, uint32_t n)
{
    uint32_t crc = 0;

    while (n--)
    {
        crc = crc16_step(crc, *p++);
    }
    return crc;
}

/* ------------------------------------------------------------ packets */

/* Sends a FW packet: 0x02, len, 36, 101, 128, sub, value(4), crc, 0x03.
 * len 5 sends only the top byte of value (the DONE status). */
static void send_fw(uint32_t sub, uint32_t value, uint32_t len)
{
    uint8_t p[8];

    p[0] = COMM_CUSTOM_APP_DATA;
    p[1] = LCM_PACKAGE_ID;
    p[2] = COMMAND_LCM_FW;
    p[3] = (uint8_t)sub;
    for (int i = 0; i < 4; i++)
    {
        p[4 + i] = (uint8_t)(value >> (24 - 8 * i));
    }
    uint32_t crc = crc16_buf(p, len);

    bl_uart_put(0x02);
    bl_uart_put(len);
    for (uint32_t i = 0; i < len; i++)
    {
        bl_uart_put(p[i]);
    }
    bl_uart_put(crc >> 8);
    bl_uart_put(crc & 0xFFu);
    bl_uart_put(0x03);
}

static uint32_t get_u32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

/* ------------------------------------------------------------- flash */

/* Erases and writes one page, then reads it back. Page 0 of the app is
 * written back to front, so its first words (SP, reset vector) go last.
 * Returns nonzero if the page reads back as intended. */
static int write_page(uint32_t addr, const uint8_t *data)
{
    bl_flash_erase(addr);
    for (uint32_t k = 0; k < PAGE_SIZE; k += 2u)
    {
        uint32_t i = (addr == APP_BASE) ? (PAGE_SIZE - 2u - k) : k;
        bl_flash_write16(addr + i, (uint32_t)data[i] | ((uint32_t)data[i + 1u] << 8));
    }
    for (uint32_t i = 0; i < PAGE_SIZE; i++)
    {
        if (bl_rd8(addr + i) != data[i])
        {
            return 0;
        }
    }
    return 1;
}

/* ------------------------------------------------------------ updater */

/* Returns (back to the app) only when an update finished and verified, or
 * when coming from the app and nothing was staged. Otherwise it stays here
 * (power held) until an image arrives, or the button is held 2 s. */
void bl_updater(int from_app)
{
    uint8_t rx[RX_MAX + 3];
    uint32_t rx_n = 0, rx_len = 0;
    uint32_t offset = PAGE_SIZE, size = 0, crc = 0;
    uint32_t wait_ms = 0, idle_ms = 0, button_ms = 0, fails = 0;
    int erased = 0, request = 1;
    int armed = !bl_button_pressed(); /* a hold only counts after a release */

    bl_power_on();
    bl_hw_init();

    for (;;)
    {
        if (request)
        {
            request = 0;
            wait_ms = 0;
            rx_n = 0;
            if (size != 0u && offset >= size)
            {
                offset = 0; /* page 0 goes last */
            }
            send_fw(LCM_FW_REQ, offset, 8);
        }

        /* ---- 1 ms housekeeping: request timeout, give-up, button */
        if (bl_tick_ms())
        {
            if (++wait_ms >= REQUEST_TIMEOUT_MS)
            {
                request = 1;
            }
            /* headlights tell the rider we're here: a 1 Hz blink while
             * waiting; steady while pages are coming in (idle_ms restarts) */
            if ((++idle_ms & 0x1FFu) == 0u)
            {
                bl_led_toggle();
            }
            if (from_app && !erased && idle_ms >= GIVE_UP_MS)
            {
                send_fw(LCM_FW_DONE, 2u << 24, 5); /* app untouched: back to it */
                return;
            }
            if (bl_button_pressed())
            {
                if (armed && ++button_ms >= BUTTON_OFF_MS)
                {
                    bl_power_off();
                    return;
                }
            }
            else
            {
                armed = 1;
                button_ms = 0;
            }
        }

        /* ---- receive: short VESC packet 0x02, len, payload, crc16, 0x03 */
        int b = bl_uart_get();
        if (b < 0)
        {
            continue;
        }
        if (rx_n == 0u)
        {
            if (b == 0x02)
            {
                rx_n = 1;
            }
            continue;
        }
        if (rx_n == 1u)
        {
            rx_len = (uint32_t)b;
            rx_n = (rx_len == 0u || rx_len > RX_MAX) ? 0u : 2u;
            continue;
        }
        rx[rx_n++ - 2u] = (uint8_t)b;
        if (rx_n < rx_len + 5u)
        {
            continue;
        }
        rx_n = 0;
        if (rx[rx_len + 2u] != 0x03u ||
            crc16_buf(rx, rx_len) != (((uint32_t)rx[rx_len] << 8) | rx[rx_len + 1u]))
        {
            continue; /* broken packet: keep listening */
        }

        /* ---- a valid packet */
        uint32_t r_size = get_u32(&rx[8]);
        uint32_t r_crc = ((uint32_t)rx[12] << 8) | rx[13];
        if (rx_len < HDR ||
            get_u32(rx) != ((COMM_CUSTOM_APP_DATA << 24) | (LCM_PACKAGE_ID << 16) |
                            (COMMAND_LCM_FW << 8) | LCM_FW_REQ))
        {
            continue; /* not for us */
        }
        if (r_size == 0u || r_size > MAX_IMAGE || (r_size % PAGE_SIZE) != 0u)
        {
            wait_ms = REQUEST_TIMEOUT_MS - RETRY_MS; /* not there yet: ask again soon */
            continue;
        }
        request = 1;
        if (r_size != size || r_crc != crc)
        {
            size = r_size; /* (new) image: start over */
            crc = r_crc;
            offset = PAGE_SIZE;
            continue;
        }
        if (get_u32(&rx[4]) != offset || rx_len != RX_MAX || fails >= MAX_FAILS)
        {
            continue; /* (after 3 bad images: no more writing until power-up) */
        }

        idle_ms = 0;
        if (!erased)
        {
            bl_flash_erase(APP_BASE); /* no valid app until we're done */
            erased = 1;
        }
        if (!write_page(APP_BASE + offset, &rx[HDR]))
        {
            continue; /* didn't read back right: ask for the same page again */
        }
        if (offset == 0u)
        {
            if (crc16_buf((const uint8_t *)bl_ptr(APP_BASE), size) == crc && bl_app_valid())
            {
                send_fw(LCM_FW_DONE, 0, 5);
                return;
            }
            bl_flash_erase(APP_BASE); /* not the image we were sent: no app */
            ++fails;
            send_fw(LCM_FW_DONE, 1u << 24, 5);
            size = 0; /* re-read size/CRC and start over */
        }
        offset += PAGE_SIZE;
    }
}

/* -------------------------------------------------------- entry points */

#ifndef BL_HOST
void Reset_Handler(void);
void Update_Entry(void);

/* Only the core vectors: interrupts are never enabled here. Slot 7
 * (reserved on Cortex-M0) holds the update entry the app calls. */
__attribute__((section(".isr_vector"), used)) void (*const vectors[8])(void) = {
    (void (*)(void))(&_estack), Reset_Handler, 0, 0, 0, 0, 0, Update_Entry,
};

void __attribute__((used, noreturn)) update_from_app(void)
{
    bl_updater(1);
    bl_start_app();
}

/* Called by the app with a plain function call (interrupts may still be
 * on): mask them and switch to our own stack before anything else. */
__attribute__((naked)) void Update_Entry(void)
{
    __asm volatile("cpsid i\n"
                   "ldr r0, =_estack\n"
                   "msr msp, r0\n"
                   "b update_from_app\n"
                   ".ltorg\n");
}

void __attribute__((noreturn)) Reset_Handler(void)
{
    if (!bl_app_valid())
    {
        bl_updater(0);
    }
    bl_start_app();
}
#endif
