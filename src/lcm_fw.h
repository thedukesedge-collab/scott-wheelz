// SPDX-License-Identifier: GPL-3.0-or-later
// Part of Refloat rink-mixed (refloatRM), 2026. See FORK_NOTES.md.
// Re-Float Rink-Mixed: LCM firmware update relay (phone -> package -> LCM
// bootloader). GPL-3.0-or-later.
//
// The phone stages an image (START: size, CRC16), the package raises the
// "update" flag in the VP block of its LCM poll replies (only while the
// board is not running), the ALCM VP app hands over to the bootloader, and
// the bootloader asks for the image page by page with COMMAND_LCM_FW
// requests over the LCM UART. The package keeps just two 128-byte pages;
// the phone polls STATUS and sends the page the LCM wants (and the next).
// See lcm/bootloader/README.md for the full protocol.
#pragma once

#include "state.h"
#include "time.h"

#include <stdbool.h>
#include <stdint.h>

#define LCM_FW_PAGE 128
#define LCM_FW_MAX_SIZE 14720  // 0x08000600 - 0x08003F80

// sub-commands of COMMAND_LCM_FW
#define LCM_FW_START 0x01  // phone: size (u32), crc (u16)
#define LCM_FW_DATA 0x02  // phone: offset (u32), 128 bytes
#define LCM_FW_STATUS 0x03  // phone: -
#define LCM_FW_CANCEL 0x04  // phone: -
#define LCM_FW_REQ 0x10  // LCM bootloader: offset (u32)
#define LCM_FW_DONE 0x11  // LCM bootloader: status (0 ok, 1 CRC mismatch, 2 nothing came)

typedef enum {
    LCM_FW_IDLE = 0,
    LCM_FW_STAGED = 1,  // waiting for the LCM to start its bootloader
    LCM_FW_SENDING = 2,  // the bootloader is pulling pages
    LCM_FW_OK = 3,  // written and verified, new app running
    LCM_FW_FAILED = 4,
} LcmFwState;

typedef enum {
    LCM_FW_ERR_NONE = 0,
    LCM_FW_ERR_RUNNING = 1,  // board running: not started
    LCM_FW_ERR_BAD_IMAGE = 2,  // size not whole pages / too big
    LCM_FW_ERR_NO_BOOTLOADER = 3,  // the LCM has no bootloader / never asked for pages
    LCM_FW_ERR_LCM_GAVE_UP = 4,  // bootloader: nothing came, back to the old app
    LCM_FW_ERR_CRC = 5,  // bootloader: image CRC mismatch (it retries)
} LcmFwError;

typedef struct {
    uint8_t state;
    uint8_t error;
    uint8_t crc_fails;
    uint16_t crc;
    uint32_t size;
    uint32_t want;  // offset the LCM asked for last
    uint32_t page_off[2];
    bool page_ok[2];
    uint8_t page[2][LCM_FW_PAGE];
    time_t staged_time;
    bool bl_seen;  // a bootloader asked for a page...
    time_t bl_seen_time;  // ...last at this time
    bool poll_seen;  // an LCM polled (COMMAND_LCM_POLL)...
    time_t poll_time;  // ...last at this time (any LCM firmware that polls)
} LcmFw;

// STATUS flags
#define LCM_FW_FLAG_BL_ACTIVE 0x01  // a bootloader asked for a page in the last few seconds
#define LCM_FW_FLAG_CAPABLE 0x02  // the LCM named itself "VPBL-..." (has the bootloader)
#define LCM_FW_FLAG_POLLING 0x04  // an LCM polled the package in the last 2 s (named or not)
#define LCM_FW_BL_RECENT_S 5.0f

void lcm_fw_init(LcmFw *fw);

// All sub-commands of COMMAND_LCM_FW, from the phone or the LCM bootloader;
// replies right away (the VESC sends a reply back on the interface the
// command came in on). `buf` starts after the package id and command.
//
// Fail-safe: START is refused (LCM_FW_ERR_NO_BOOTLOADER) unless the LCM
// named itself as an ALCM VP build with the bootloader (`has_bootloader`,
// its poll name starts with "VPBL") or a bootloader is asking for pages
// right now (recovery after an interrupted update). An LCM without the
// bootloader is never sent the update flag.
void lcm_fw_command(
    LcmFw *fw,
    const State *state,
    const Time *time,
    bool has_bootloader,
    const uint8_t *buf,
    int len
);

// True while the VP block should carry the update flag (staged, board not
// running). Also times out a staged update the LCM never picks up.
bool lcm_fw_update_flag(LcmFw *fw, const State *state, const Time *time);

// rf77 safety: true while the board must not start - an update is staged
// (the LCM is about to hand over to its bootloader) or a bootloader asked
// for a page in the last few seconds (update running, or an LCM stuck in its
// bootloader after an interrupted update). Read-only, for the balance thread.
bool lcm_fw_blocks_engage(const LcmFw *fw, const Time *time);

// An LCM poll arrived (for the STATUS "polling" flag: tells "no LCM talking"
// apart from "an LCM without a name").
void lcm_fw_poll_seen(LcmFw *fw, const Time *time);
