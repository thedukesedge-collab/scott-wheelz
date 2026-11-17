#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "lcm.h"
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
    uint8_t in[11] = {0x06, 80, 30, 90, 4, 6, 9, 11, 0, 0x01, 0x2C};
    assert(lcm_vp_set(&lcm, in, 11) == true);
    assert(lcm_vp_set(&lcm, in, 11) == false);  /* same again: no change */
    lcm_poll_response(&lcm, &st, 0, &md, 0);
    /* package buffer = ALCM payload minus the leading COMM_CUSTOM_APP_DATA (36)
       byte, so ALCM offset N is package index N-1; ALCM sees 25 bytes */
    assert(sent_len == 24);
    assert(sent[11]==80 && sent[12]==80 && sent[13]==30);  /* idle: status idle % */
    assert(sent[14]==0x56 && sent[15]==1 && sent[16]==0x06);
    assert(sent[17]==4 && sent[18]==6 && sent[19]==9 && sent[20]==11 && sent[21]==0);
    assert(sent[22]==0x01 && sent[23]==0x2C);
    /* riding: status riding % */
    st.state = STATE_RUNNING; lcm_poll_response(&lcm, &st, 0, &md, 0);
    assert(sent[13]==90 && sent_len==24);
    assert(sent[11]==80 && sent[12]==80);  /* 11-byte form: idle headlight = riding */
    /* 12-byte form: headlight idle 25 % - riding 80 %, idle 25 % */
    uint8_t in12[12] = {0x06, 80, 30, 90, 4, 6, 9, 11, 0, 0x01, 0x2C, 25};
    assert(lcm_vp_set(&lcm, in12, 12) == true);
    lcm_poll_response(&lcm, &st, 0, &md, 0);
    assert(sent[11]==80 && sent[12]==25 && sent[13]==90);
    st.state = STATE_READY; lcm_poll_response(&lcm, &st, 0, &md, 0);
    assert(sent[11]==25 && sent[12]==25 && sent[13]==30);
    /* test beep toggles flag bit 3, not stored */
    lcm_vp_test_beep(&lcm); lcm_poll_response(&lcm, &st, 0, &md, 0);
    assert(sent[16]==(0x06|0x08));
    lcm_vp_test_beep(&lcm); lcm_poll_response(&lcm, &st, 0, &md, 0);
    assert(sent[16]==0x06);
    /* clamping: 150 %, anim 99, color 999, unknown flag bits */
    uint8_t bad[11] = {0xFF, 150, 150, 150, 99, 99, 99, 99, 99, 0x03, 0xE7};
    lcm_vp_set(&lcm, bad, 11);
    assert(lcm.vp.flags==0x07 && lcm.vp.headlight_brightness==100 && lcm.vp.animations[0]==12 && lcm.vp.color==359);
    /* short command ignored */
    assert(lcm_vp_set(&lcm, in, 10) == false && lcm.vp.color==359);
    /* pack/unpack round trip */
    lcm_vp_set(&lcm, in12, 12);
    uint32_t w[LCM_VP_EEPROM_WORDS]; lcm_vp_pack(&lcm, w);
    LcmData l2; memset(&l2,0,sizeof l2); lcm_init(&l2, &hw);
    lcm_vp_unpack(&l2, w);
    assert(l2.vp.valid && memcmp(&l2.vp, &lcm.vp, sizeof lcm.vp)==0 && l2.vp.headlight_idle_brightness==25);
    /* settings saved before word 3 existed: idle = riding */
    uint32_t old[LCM_VP_EEPROM_WORDS] = {w[0], w[1], w[2], 0};
    LcmData l4; memset(&l4,0,sizeof l4); lcm_vp_unpack(&l4, old);
    assert(l4.vp.valid && l4.vp.headlight_idle_brightness==80);
    /* bad magic: not loaded */
    w[0] &= 0x00FFFFFF; LcmData l3; memset(&l3,0,sizeof l3); lcm_vp_unpack(&l3, w); assert(!l3.vp.valid);
    printf("lcm VP tests passed (reply %u bytes + command byte = %u at ALCM, max 32)\n", sent_len, sent_len + 1);
    return 0;
}
