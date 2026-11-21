// SPDX-License-Identifier: GPL-3.0-or-later
// Part of Refloat rink-mixed (refloatRM), 2026. See FORK_NOTES.md.

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "lcm.h"
#include "battery_curve.h"
#include "vesc_c_if.h"
static uint8_t sent[128]; static unsigned sent_len;
static int f(void){return 0;} static float c(void){return 1.5f;} static float v(void){return 84.2f;}
static void send(unsigned char *b, unsigned n){ memcpy(sent,b,n); sent_len=n; }
static float bl(float *w){return 0.5f;} static vesc_c_if iface = { f, c, v, send, bl }; vesc_c_if *VESC_IF = &iface;
void log_error(const char *fmt, ...){} void fatal_error_terminate(void){}
int main(void){
    LcmData lcm; memset(&lcm,0,sizeof lcm);
    CfgHwLeds hw = { 0x1 };  /* LED mode INTERNAL: not external */
    lcm_init(&lcm, &hw);
    State st = { STATE_READY, MODE_NORMAL }; MotorData md = {0};
    /* 1: no VP settings, not external -> header only */
    lcm_poll_response(&lcm, &st, 0, &md, 0);
    assert(sent_len == 2 && sent[0]==101 && sent[1]==24);
    /* 2: VP settings: headlights off, bar on, beep on; 80/30/90 %; anims; color 300 */
    uint8_t in[11] = {0x27, 80, 30, 90, 4, 6, 9, 11, 0, 0x01, 0x2C};
    assert(lcm_vp_set(&lcm, in, 11) == true);
    assert(lcm_vp_set(&lcm, in, 11) == false);  /* same again: no change */
    /* an LCM that names itself as something else gets no VP block (stock) */
    uint8_t other[] = "OtherLCM 2.0"; lcm_poll_request(&lcm, other, sizeof other - 1);
    lcm_poll_response(&lcm, &st, 0, &md, 0);
    assert(sent_len == 2);
    /* an unnamed LCM (pre-r15 SLCM) gets the VP block */
    LcmData un; memset(&un, 0, sizeof un); lcm_init(&un, &hw); lcm_vp_set(&un, in, 11);
    lcm_poll_response(&un, &st, 0, &md, 0);
    assert(sent_len == 25);
    /* SLCM names itself: VP block */
    uint8_t nm[] = "VPBL-r15-79f773f"; lcm_poll_request(&lcm, nm, sizeof nm - 1);
    assert(strcmp(lcm.name, "VPBL-r15-79f773f") == 0);
    lcm_poll_response(&lcm, &st, 0, &md, 0);
    /* package buffer = ALCM payload minus the leading COMM_CUSTOM_APP_DATA (36)
       byte, so ALCM offset N is package index N-1; ALCM sees 25 bytes */
    assert(sent_len == 25);
    assert(sent[11]==80 && sent[12]==80 && sent[13]==30);  /* idle: status idle % */
    assert(sent[14]==0x56 && sent[15]==1 && sent[16]==0x27);  /* doze-dark (0x20) kept */
    assert(sent[17]==4 && sent[18]==6 && sent[19]==9 && sent[20]==11 && sent[21]==0);
    assert(sent[22]==0x01 && sent[23]==0x2C);
    assert(sent[24]==0xFF);  /* no battery curve: the LCM uses the VESC's % */
    lcm.vp_battery = 57; lcm_poll_response(&lcm, &st, 0, &md, 0);
    assert(sent[24]==57); lcm.vp_battery = 0xFF;
    /* riding: status riding % */
    st.state = STATE_RUNNING; lcm_poll_response(&lcm, &st, 0, &md, 0);
    assert(sent[13]==90 && sent_len==25);
    assert(sent[11]==80 && sent[12]==80);  /* 11-byte form: idle headlight = riding */
    /* 12-byte form: headlight idle 25 % - riding 80 %, idle 25 % */
    uint8_t in12[12] = {0x27, 80, 30, 90, 4, 6, 9, 11, 0, 0x01, 0x2C, 25};
    assert(lcm_vp_set(&lcm, in12, 12) == true);
    lcm_poll_response(&lcm, &st, 0, &md, 0);
    assert(sent[11]==80 && sent[12]==25 && sent[13]==90);
    st.state = STATE_READY; lcm_poll_response(&lcm, &st, 0, &md, 0);
    assert(sent[11]==25 && sent[12]==25 && sent[13]==30);
    /* test beep toggles flag bit 3, not stored */
    lcm_vp_test_beep(&lcm); lcm_poll_response(&lcm, &st, 0, &md, 0);
    assert(sent[16]==(0x27|0x08));
    lcm_vp_test_beep(&lcm); lcm_poll_response(&lcm, &st, 0, &md, 0);
    assert(sent[16]==0x27);
    /* headlights off: brightness 0 riding and idle (off really is off) */
    uint8_t off12[12] = {0x06, 80, 30, 90, 4, 6, 9, 11, 0, 0x01, 0x2C, 25};
    assert(lcm_vp_set(&lcm, off12, 12) == true);
    lcm_poll_response(&lcm, &st, 0, &md, 0);
    assert(sent[11]==0 && sent[12]==0 && sent[13]==30);
    st.state = STATE_RUNNING; lcm_poll_response(&lcm, &st, 0, &md, 0);
    assert(sent[11]==0 && sent[12]==0 && sent[13]==90);
    st.state = STATE_READY;
    /* battery gauge (13) and custom (14) accepted, 15 clamped */
    uint8_t bat12[12] = {0x07, 80, 30, 90, 13, 14, 13, 15, 0, 0x01, 0x2C, 25};
    assert(lcm_vp_set(&lcm, bat12, 12) == true);
    lcm_poll_response(&lcm, &st, 0, &md, 0);
    assert(sent[17]==13 && sent[18]==14 && sent[19]==13 && sent[20]==14);
    /* CUSTOM block: 20-byte command, sent only to an SLCM r19+ */
    uint8_t cus[20] = {0x07, 80, 30, 90, 14, 14, 0, 11, 0, 0x01, 0x2C, 25, 0x13, 1, 10, 100, 20, 9, 40, 0};
    assert(lcm_vp_set(&lcm, cus, 20) == true);
    assert(lcm.vp.custom[0]==0x13 && lcm.vp.custom[6]==40);
    lcm_poll_response(&lcm, &st, 0, &md, 0);
    assert(sent_len == 25);                       /* r15: no custom block */
    uint8_t n19[] = "VPBL-r19-abcdef0"; lcm_poll_request(&lcm, n19, sizeof n19 - 1);
    assert(lcm_slcm_rev(&lcm) == 19);
    lcm_poll_response(&lcm, &st, 0, &md, 0);
    assert(sent_len == 33 && sent[25]==0x13 && sent[31]==40);  /* 34 at the LCM */
    uint8_t n7[] = "VP-r7-abcdef0"; lcm_poll_request(&lcm, n7, sizeof n7 - 1);
    assert(lcm_slcm_rev(&lcm) == 7);
    /* idle following Refloat's Status Idle animation (rf72) */
    {
        CfgLeds cl; memset(&cl, 0, sizeof cl);
        lcm_poll_request(&lcm, n19, sizeof n19 - 1);
        lcm.leds_cfg = &cl;
        uint8_t fol[20]; memcpy(fol, cus, 20); fol[4] = LCM_VP_ANIM_REFLOAT;
        assert(lcm_vp_set(&lcm, fol, 20) && lcm.vp.animations[0] == LCM_VP_ANIM_REFLOAT);
        lcm_poll_response(&lcm, &st, 0, &md, 0);
        assert(sent[17] == 0);                       /* timeout 0 (Refloat default): STANDARD */
        cl.status.idle_timeout = 5; cl.status_idle.mode = LED_ANIM_KNIGHT_RIDER;
        cl.status_idle.color1 = COLOR_RED; cl.status_idle.color2 = COLOR_BLUE; cl.status_idle.speed = 1.0f;
        lcm_poll_response(&lcm, &st, 0, &md, 0);
        assert(sent[17] == 14 && sent[25] == (1 | (2 << 1) | (6 << 4)) && sent[27] == 0 && sent[28] == 0);
        assert(sent[29] == 19 && sent[30] == 19);   /* 1 s = 20 x 50 ms */
        st.state = STATE_RUNNING; lcm_poll_response(&lcm, &st, 0, &md, 0);
        assert(sent[25] == 0x13);                    /* riding: the rider's own CUSTOM */
        st.state = STATE_READY;
        cl.status_idle.mode = LED_ANIM_FADE;
        lcm_poll_response(&lcm, &st, 0, &md, 0);
        assert(sent[17] == 14 && sent[27] == 0 && sent[28] == 120);  /* red -> blue */
        cl.status_idle.mode = LED_ANIM_FELONY;
        lcm_poll_response(&lcm, &st, 0, &md, 0);
        assert(sent[17] == 5 && sent[25] == 0x13);  /* THE FUZZ, own custom block kept */
        cl.status_idle.mode = LED_ANIM_SOLID; cl.status_idle.color1 = COLOR_WHITE_FULL;
        lcm_poll_response(&lcm, &st, 0, &md, 0);
        assert(sent[17] == 14 && sent[25] == (4 << 1));  /* white: the SLCM's own color */
        lcm_poll_request(&lcm, n7, sizeof n7 - 1);        /* older SLCM: no CUSTOM */
        lcm_poll_response(&lcm, &st, 0, &md, 0);
        assert(sent[17] == 0 && sent_len == 25);
        lcm_poll_request(&lcm, n19, sizeof n19 - 1);
        assert(lcm_vp_set(&lcm, cus, 20));
        lcm.leds_cfg = NULL;
    }
    /* rf77: the power-off bit (0x40) is never sent, idle or riding */
    {
        lcm_poll_response(&lcm, &st, 0, &md, 0);
        assert(!(sent[16] & 0x40));
        st.state = STATE_RUNNING; lcm_poll_response(&lcm, &st, 0, &md, 0);
        assert(!(sent[16] & 0x40));
        st.state = STATE_READY;
    }
    /* an older app (12 bytes) keeps the custom block */
    assert(lcm_vp_set(&lcm, in12, 12) == true && lcm.vp.custom[0]==0x13);
    lcm_poll_request(&lcm, n19, sizeof n19 - 1);
    /* clamping: 150 %, anim 99, color 999, unknown flag bits */
    uint8_t bad[11] = {0xFF, 150, 150, 150, 99, 99, 99, 99, 99, 0x03, 0xE7};
    lcm_vp_set(&lcm, bad, 11);
    assert(lcm.vp.flags==0x27 && lcm.vp.headlight_brightness==100 && lcm.vp.animations[0]==14 && lcm.vp.color==359);
    /* short command ignored */
    assert(lcm_vp_set(&lcm, in, 10) == false && lcm.vp.color==359);
    /* pack/unpack round trip */
    lcm_vp_set(&lcm, in12, 12);
    uint32_t w[LCM_VP_EEPROM_WORDS]; lcm_vp_pack(&lcm, w);
    LcmData l2; memset(&l2,0,sizeof l2); lcm_init(&l2, &hw);
    lcm_vp_unpack(&l2, w);
    assert(l2.vp.valid && memcmp(&l2.vp, &lcm.vp, sizeof lcm.vp)==0 && l2.vp.headlight_idle_brightness==25);
    /* settings saved before word 3 existed: idle = riding */
    assert(l2.vp.custom[0]==0x13 && l2.vp.custom[6]==40);  /* custom words 4, 5 */
    uint32_t old[LCM_VP_EEPROM_WORDS] = {w[0], w[1], w[2], 0, 0, 0};
    LcmData l4; memset(&l4,0,sizeof l4); lcm_vp_unpack(&l4, old);
    assert(l4.vp.valid && l4.vp.headlight_idle_brightness==80);
    /* rf86: snooze minutes (byte 20), clamped to 60, kept by shorter commands, stored in word 3 */
    {
        uint8_t in21[21]; memcpy(in21, in12, 12); memcpy(in21 + 12, lcm.vp.custom, 8); in21[20] = 99;
        assert(lcm_vp_set(&lcm, in21, 21) == true && lcm.vp.snooze_minutes == 60);
        in21[20] = 5; lcm_vp_set(&lcm, in21, 21); assert(lcm.vp.snooze_minutes == 5);
        lcm_vp_set(&lcm, in21, 20); assert(lcm.vp.snooze_minutes == 5);
        lcm_vp_set(&lcm, in12, 12); assert(lcm.vp.snooze_minutes == 5);
        uint32_t ws[LCM_VP_EEPROM_WORDS]; lcm_vp_pack(&lcm, ws);
        LcmData l5; memset(&l5,0,sizeof l5); lcm_init(&l5, &hw); lcm_vp_unpack(&l5, ws);
        assert(l5.vp.snooze_minutes == 5 && memcmp(&l5.vp, &lcm.vp, sizeof lcm.vp) == 0);
        in21[20] = 0; lcm_vp_set(&lcm, in21, 21);
    }
    /* rf97: AUTO snooze (255), ride mode (byte 21, max 9), snooze style (byte 22, max 2) */
    {
        uint8_t in23[23]; memcpy(in23, in12, 12); memcpy(in23 + 12, lcm.vp.custom, 8);
        in23[20] = 255; in23[21] = 99; in23[22] = 7;
        lcm_vp_set(&lcm, in23, 23);
        assert(lcm.vp.snooze_minutes == 255 && lcm.vp.ride_mode == 9 && lcm.vp.snooze_style == 2);
        in23[21] = 5; in23[22] = 1; lcm_vp_set(&lcm, in23, 23);
        lcm_vp_set(&lcm, in23, 21); assert(lcm.vp.ride_mode == 5 && lcm.vp.snooze_style == 1);
        uint32_t ws[LCM_VP_EEPROM_WORDS]; lcm_vp_pack(&lcm, ws);
        LcmData l6; memset(&l6,0,sizeof l6); lcm_init(&l6, &hw); lcm_vp_unpack(&l6, ws);
        assert(memcmp(&l6.vp, &lcm.vp, sizeof lcm.vp) == 0 && l6.vp.color == lcm.vp.color);
        in23[20] = 0; in23[21] = 0; in23[22] = 0; lcm_vp_set(&lcm, in23, 23);
    }
    /* rf109: status bar snooze (byte 23, max 3), word 82 bits 9-11 */
    {
        uint8_t in24[24]; memcpy(in24, in12, 12); memcpy(in24 + 12, lcm.vp.custom, 8);
        in24[20] = 5; in24[21] = 2; in24[22] = 1; in24[23] = 99;
        lcm_vp_set(&lcm, in24, 24); assert(lcm.vp.status_snooze == 12);  /* rf111: up to 12 */
        in24[23] = 11; lcm_vp_set(&lcm, in24, 24);
        { uint32_t w11[LCM_VP_EEPROM_WORDS]; lcm_vp_pack(&lcm, w11);
          LcmData l8; memset(&l8,0,sizeof l8); lcm_init(&l8, &hw); lcm_vp_unpack(&l8, w11);
          assert(l8.vp.status_snooze == 11 && l8.vp.ride_mode == 2 && memcmp(&l8.vp, &lcm.vp, sizeof lcm.vp) == 0);
          /* an rf109 save: status in word 2 bits 9-11, word 3 nibble 0 */
          w11[3] &= ~(0x0Fu << 20); w11[2] |= 2u << 9;
          LcmData l9; memset(&l9,0,sizeof l9); lcm_init(&l9, &hw); lcm_vp_unpack(&l9, w11);
          assert(l9.vp.status_snooze == 2 && l9.vp.color == lcm.vp.color); }
        /* rf114: riding / snooze colors and speeds (bytes 24-29, word 6) */
        { uint8_t in30[30]; memcpy(in30, in24, 24); in30[24] = 4; in30[25] = 99; in30[26] = 60; in30[27] = 31; in30[28] = 0; in30[29] = 7;
          lcm_vp_set(&lcm, in30, 30);
          assert(lcm.vp.ride_c1 == 4 && lcm.vp.ride_c2 == 31 && lcm.vp.ride_speed == 50 && lcm.vp.snooze_c1 == 31 && lcm.vp.snooze_speed == 7);
          lcm_vp_set(&lcm, in30, 24); assert(lcm.vp.ride_speed == 50 && lcm.vp.snooze_speed == 7);
          uint32_t w14[LCM_VP_EEPROM_WORDS]; lcm_vp_pack(&lcm, w14);
          LcmData l10; memset(&l10,0,sizeof l10); lcm_init(&l10, &hw); lcm_vp_unpack(&l10, w14);
          assert(memcmp(&l10.vp, &lcm.vp, sizeof lcm.vp) == 0);
          w14[6] = 0; LcmData l11; memset(&l11,0,sizeof l11); lcm_init(&l11, &hw); lcm_vp_unpack(&l11, w14);
          assert(l11.vp.ride_speed == 0 && l11.vp.snooze_speed == 0); }
        in24[23] = 2; lcm_vp_set(&lcm, in24, 24);
        lcm_vp_set(&lcm, in24, 23); assert(lcm.vp.status_snooze == 2);
        uint32_t ws[LCM_VP_EEPROM_WORDS]; lcm_vp_pack(&lcm, ws);
        LcmData l7; memset(&l7,0,sizeof l7); lcm_init(&l7, &hw); lcm_vp_unpack(&l7, ws);
        assert(memcmp(&l7.vp, &lcm.vp, sizeof lcm.vp) == 0);
    }
    /* bad magic: not loaded */
    w[0] &= 0x00FFFFFF; LcmData l3; memset(&l3,0,sizeof l3); lcm_vp_unpack(&l3, w); assert(!l3.vp.valid);
    /* battery curve: p42a points (cell mV at 100..0 %) */
    {
        BatteryCurve bc; battery_curve_init(&bc);
        uint8_t set[13] = {1, 0, 170, 156, 144, 135, 127, 119, 111, 104, 96, 84, 50};
        assert(battery_curve_set(&bc, set, 13));
        assert(!battery_curve_set(&bc, set, 13));       /* unchanged */
        assert(battery_curve_update(&bc, 0, 0, 18) == 0xFF);   /* no voltage */
        BatteryCurve b2; battery_curve_init(&b2); battery_curve_set(&b2, set, 13);
        assert(battery_curve_update(&b2, 4.2f * 18, 0, 18) == 100);
        BatteryCurve b3; battery_curve_init(&b3); battery_curve_set(&b3, set, 13);
        assert(battery_curve_update(&b3, 3.690f * 18, 0, 18) == 50);  /* exactly the 50 % point */
        BatteryCurve b4; battery_curve_init(&b4); battery_curve_set(&b4, set, 13);
        uint8_t q = battery_curve_update(&b4, 3.730f * 18, 0, 18);   /* halfway 50..60 % (3.69..3.77 V) */
        assert(q == 55);
        BatteryCurve b5; battery_curve_init(&b5); battery_curve_set(&b5, set, 13);
        assert(battery_curve_update(&b5, 2.9f * 18, 0, 18) == 0);
        /* sag compensation: 3.60 V/cell at 20 A with 126 mOhm (63 x 2) -> +2.52 V */
        uint8_t sag[13]; memcpy(sag, set, 13); sag[1] = 63;
        BatteryCurve b6; battery_curve_init(&b6); battery_curve_set(&b6, sag, 13);
        uint8_t r6 = battery_curve_update(&b6, 3.60f * 18 - 2.52f, 20.0f, 18);
        BatteryCurve b7; battery_curve_init(&b7); battery_curve_set(&b7, set, 13);
        assert(r6 == battery_curve_update(&b7, 3.60f * 18, 0, 18));
        /* rising curve rejected */
        uint8_t bad[13]; memcpy(bad, set, 13); bad[6] = bad[5];
        BatteryCurve b8; battery_curve_init(&b8);
        assert(!battery_curve_set(&b8, bad, 13) && !b8.enabled);
        /* off -> 0xFF; EEPROM round trip */
        uint32_t w[4]; battery_curve_pack(&bc, w);
        BatteryCurve b9; battery_curve_init(&b9); battery_curve_unpack(&b9, w);
        assert(b9.enabled && memcmp(b9.points, bc.points, 11) == 0);
        set[0] = 0; battery_curve_set(&bc, set, 13);
        assert(battery_curve_update(&bc, 70, 0, 18) == 0xFF);
        printf("battery curve tests passed\n");
    }
    printf("lcm VP tests passed (reply %u bytes + command byte = %u at ALCM, max 40 from SLCM r19)\n", sent_len, sent_len + 1);
    return 0;
}
