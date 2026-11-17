// Re-Float Rink-Mixed: LCM firmware update relay. GPL-3.0-or-later.
// See lcm_fw.h.

#include "lcm_fw.h"

#include "vesc_c_if.h"

#include <string.h>

#define COMMAND_LCM_FW 128
#define STAGED_TIMEOUT_S 15.0f  // the LCM must start its bootloader by then

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

// STATUS reply: 101, 128, 0x03, state, error, wanted offset (u32), image
// size (u32) - 13 bytes
static void send_status(const LcmFw *fw) {
    uint8_t b[13];
    b[0] = 101;
    b[1] = COMMAND_LCM_FW;
    b[2] = LCM_FW_STATUS;
    b[3] = fw->state;
    b[4] = fw->error;
    put_u32(&b[5], fw->want);
    put_u32(&b[9], fw->size);
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

void lcm_fw_command(LcmFw *fw, const State *state, const Time *time, const uint8_t *buf, int len) {
    if (len < 1) {
        return;
    }

    switch (buf[0]) {
    case LCM_FW_START:
        if (len < 7) {
            return;
        }
        lcm_fw_init(fw);
        fw->size = get_u32(&buf[1]);
        fw->crc = ((uint16_t) buf[5] << 8) | buf[6];
        fw->want = LCM_FW_PAGE;
        if (state->state == STATE_RUNNING) {
            fw->state = LCM_FW_FAILED;
            fw->error = LCM_FW_ERR_RUNNING;
        } else if (fw->size == 0 || fw->size > LCM_FW_MAX_SIZE || fw->size % LCM_FW_PAGE) {
            fw->state = LCM_FW_FAILED;
            fw->error = LCM_FW_ERR_BAD_IMAGE;
        } else {
            fw->state = LCM_FW_STAGED;
            fw->staged_time = time->now;
        }
        send_status(fw);
        return;

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
        send_status(fw);
        return;
    }

    case LCM_FW_STATUS:
        lcm_fw_update_flag(fw, state, time);  // (runs the staged timeout)
        send_status(fw);
        return;

    case LCM_FW_CANCEL:
        // An LCM already in its bootloader keeps waiting (power held) or
        // goes back to its old app after 20 s if it hadn't erased anything.
        lcm_fw_init(fw);
        send_status(fw);
        return;

    case LCM_FW_REQ:
        if (len < 5) {
            return;
        }
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
    }
}

bool lcm_fw_update_flag(LcmFw *fw, const State *state, const Time *time) {
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
