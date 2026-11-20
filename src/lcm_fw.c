// Re-Float Rink-Mixed: LCM firmware update relay. GPL-3.0-or-later.
// See lcm_fw.h.

#include "lcm_fw.h"

#include "vesc_c_if.h"

#include <string.h>

#define COMMAND_LCM_FW 128
#define STAGED_TIMEOUT_S 15.0f  // the LCM must start its bootloader by then
#define SENDING_TIMEOUT_S 10.0f  // SENDING but no page request for this long: gone

static void put_u32(uint8_t *p, uint32_t v) {
    p[0] = v >> 24;
    p[1] = v >> 16;
    p[2] = v >> 8;
    p[3] = v;
}

static uint32_t get_u32(const uint8_t *p) {
    return ((uint32_t) p[0] << 24) | ((uint32_t) p[1] << 16) | ((uint32_t) p[2] << 8) | p[3];
}

void lcm_fw_init(LcmFw *fw) {
    memset(fw, 0, sizeof(*fw));
}

static bool bl_active(const LcmFw *fw, const Time *time) {
    return fw->bl_seen && !timer_older(time, fw->bl_seen_time, LCM_FW_BL_RECENT_S);
}

// STATUS reply: 101, 128, 0x03, state, error, wanted offset (u32), image
// size (u32), flags - 14 bytes
static void send_status(const LcmFw *fw, const Time *time, bool has_bootloader) {
    uint8_t b[14];
    b[0] = 101;
    b[1] = COMMAND_LCM_FW;
    b[2] = LCM_FW_STATUS;
    b[3] = fw->state;
    b[4] = fw->error;
    put_u32(&b[5], fw->want);
    put_u32(&b[9], fw->size);
    b[13] = (bl_active(fw, time) ? LCM_FW_FLAG_BL_ACTIVE : 0) |
        (has_bootloader ? LCM_FW_FLAG_CAPABLE : 0) |
        (fw->poll_seen && !timer_older(time, fw->poll_time, 2.0f) ? LCM_FW_FLAG_POLLING : 0);
    VESC_IF->send_app_data(b, sizeof(b));
}

// Reply to a bootloader page request: the page if we have it, otherwise
// size 0 ("not there yet", the bootloader asks again 20 ms later).
static void send_page(const LcmFw *fw, uint32_t offset) {
    uint8_t b[3 + 4 + 4 + 2 + LCM_FW_PAGE];
    int slot = (offset / LCM_FW_PAGE) & 1;
    bool have = fw->state == LCM_FW_SENDING && fw->page_ok[slot] && fw->page_off[slot] == offset;

    b[0] = 101;
    b[1] = COMMAND_LCM_FW;
    b[2] = LCM_FW_REQ;
    put_u32(&b[3], offset);
    put_u32(&b[7], have ? fw->size : 0);
    b[11] = fw->crc >> 8;
    b[12] = fw->crc & 0xFF;
    if (have) {
        memcpy(&b[13], fw->page[slot], LCM_FW_PAGE);
        VESC_IF->send_app_data(b, sizeof(b));
    } else {
        VESC_IF->send_app_data(b, 13);
    }
}

void lcm_fw_command(
    LcmFw *fw,
    const State *state,
    const Time *time,
    bool has_bootloader,
    const uint8_t *buf,
    int len
) {
    if (len < 1) {
        return;
    }

    switch (buf[0]) {
    case LCM_FW_START: {
        if (len < 7) {
            return;
        }
        bool seen = fw->bl_seen, polled = fw->poll_seen;
        time_t seen_time = fw->bl_seen_time, poll_time = fw->poll_time;
        lcm_fw_init(fw);
        fw->bl_seen = seen;
        fw->bl_seen_time = seen_time;
        fw->poll_seen = polled;
        fw->poll_time = poll_time;
        fw->size = get_u32(&buf[1]);
        fw->crc = ((uint16_t) buf[5] << 8) | buf[6];
        fw->want = LCM_FW_PAGE;
        if (state->state == STATE_RUNNING) {
            fw->state = LCM_FW_FAILED;
            fw->error = LCM_FW_ERR_RUNNING;
        } else if (fw->size == 0 || fw->size > LCM_FW_MAX_SIZE || fw->size % LCM_FW_PAGE) {
            fw->state = LCM_FW_FAILED;
            fw->error = LCM_FW_ERR_BAD_IMAGE;
        } else if (!has_bootloader && !bl_active(fw, time)) {
            // stock / ALCM VP without the bootloader: it would ignore the
            // update flag at best, so don't even raise it
            fw->state = LCM_FW_FAILED;
            fw->error = LCM_FW_ERR_NO_BOOTLOADER;
        } else {
            fw->state = LCM_FW_STAGED;
            fw->staged_time = time->now;
        }
        send_status(fw, time, has_bootloader);
        return;
    }

    case LCM_FW_DATA: {
        if (len < 5 + LCM_FW_PAGE) {
            return;
        }
        uint32_t offset = get_u32(&buf[1]);
        if ((fw->state == LCM_FW_STAGED || fw->state == LCM_FW_SENDING) && offset < fw->size &&
            offset % LCM_FW_PAGE == 0) {
            int slot = (offset / LCM_FW_PAGE) & 1;
            memcpy(fw->page[slot], &buf[5], LCM_FW_PAGE);
            fw->page_off[slot] = offset;
            fw->page_ok[slot] = true;
        }
        send_status(fw, time, has_bootloader);
        return;
    }

    case LCM_FW_STATUS:
        lcm_fw_update_flag(fw, state, time);  // (runs the staged timeout)
        send_status(fw, time, has_bootloader);
        return;

    case LCM_FW_CANCEL:
        // An LCM already in its bootloader keeps waiting (power held) or
        // goes back to its old app after 20 s if it hadn't erased anything.
        {
            bool seen = fw->bl_seen, polled = fw->poll_seen;
            time_t seen_time = fw->bl_seen_time, poll_time = fw->poll_time;
            lcm_fw_init(fw);
            fw->bl_seen = seen;
            fw->bl_seen_time = seen_time;
            fw->poll_seen = polled;
            fw->poll_time = poll_time;
        }
        send_status(fw, time, has_bootloader);
        return;

    case LCM_FW_REQ:
        if (len < 5) {
            return;
        }
        fw->bl_seen = true;
        fw->bl_seen_time = time->now;
        if (fw->state == LCM_FW_STAGED) {
            fw->state = LCM_FW_SENDING;  // the bootloader is up: stop flagging
        }
        fw->want = get_u32(&buf[1]);
        send_page(fw, fw->want);
        return;

    case LCM_FW_DONE:
        if (len < 2) {
            return;
        }
        if (buf[1] == 0) {
            fw->state = LCM_FW_OK;
            fw->error = LCM_FW_ERR_NONE;
        } else if (buf[1] == 1) {
            fw->error = LCM_FW_ERR_CRC;  // the bootloader starts over by itself
            if (fw->crc_fails < 255) {
                fw->crc_fails++;
            }
        } else {
            fw->state = LCM_FW_FAILED;
            fw->error = LCM_FW_ERR_LCM_GAVE_UP;
        }
        return;

    default:
        return;  // unknown sub-command: ignored
    }
}

void lcm_fw_poll_seen(LcmFw *fw, const Time *time) {
    fw->poll_seen = true;
    fw->poll_time = time->now;
}

bool lcm_fw_blocks_engage(const LcmFw *fw, const Time *time) {
    if (fw->state == LCM_FW_STAGED && !timer_older(time, fw->staged_time, STAGED_TIMEOUT_S)) {
        return true;
    }
    return bl_active(fw, time);
}

bool lcm_fw_update_flag(LcmFw *fw, const State *state, const Time *time) {
    // rf77: SENDING ends with DONE from the bootloader; if that never comes
    // and the bootloader stopped asking (it went back to the app, or the
    // board was switched off), don't report SENDING forever
    if (fw->state == LCM_FW_SENDING && fw->bl_seen &&
        timer_older(time, fw->bl_seen_time, SENDING_TIMEOUT_S)) {
        fw->state = LCM_FW_FAILED;
        fw->error = LCM_FW_ERR_LCM_GAVE_UP;
    }
    if (fw->state != LCM_FW_STAGED) {
        return false;
    }
    if (timer_older(time, fw->staged_time, STAGED_TIMEOUT_S)) {
        fw->state = LCM_FW_FAILED;
        fw->error = LCM_FW_ERR_NO_BOOTLOADER;
        return false;
    }
    return state->state != STATE_RUNNING;
}
