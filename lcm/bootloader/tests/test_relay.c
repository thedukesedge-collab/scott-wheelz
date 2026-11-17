/*
 * End-to-end host test: phone -> VP package (the real src/lcm_fw.c) -> LCM
 * bootloader (the real bootloader.c), over simulated BLE and UART links.
 *
 * The simulated phone does what the app's UPDATE LCM button does: START,
 * then polls STATUS and sends the page the LCM wants plus the next one.
 * Checks: the update flag is raised only while staged and not running and
 * drops once the bootloader asks for pages; the image arrives intact; a
 * cut-off phone leaves no partial app and a power-up + new phone session
 * finishes the job; START is refused while running; a missing bootloader
 * times out with NO_BOOTLOADER.
 */
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>

#include "bl_host.h"
#include "lcm_fw.h"

#define APP_BASE 0x08000600u
#define APP_OFF (APP_BASE - SIM_FLASH_BASE)
#define MAXIMG (0x08003F80u - APP_BASE)
#define PAGE 128u

uint8_t sim_flash[SIM_FLASH_SIZE];
jmp_buf sim_exit;
int sim_power;
long sim_flash_ops, sim_cut_at = -1;
int sim_bad_write;

static long sim_ms, sim_calls;
static LcmFw fw;
static State st;
static Time tm;
static int phone_on = 1;
static long phone_stop_ms = -1; /* phone disappears at this time */
static int pages_sent;

/* ------------------------------------------------- package <-> both links */

static enum { TO_PHONE, TO_LCM } reply_to;
static uint8_t to_lcm[8192];
static int to_lcm_head, to_lcm_tail;
static uint8_t phone_rx[64];
static int phone_rx_len;

static uint16_t crc16(const uint8_t *p, uint32_t n)
{
    uint32_t crc = 0;
    while (n--)
    {
        crc ^= (uint32_t)*p++ << 8;
        for (int i = 0; i < 8; i++)
        {
            crc <<= 1;
            if (crc & 0x10000u)
                crc ^= 0x1021u;
        }
    }
    return (uint16_t)crc;
}

static void send_app_data(unsigned char *d, unsigned int n)
{
    if (reply_to == TO_PHONE)
    {
        memcpy(phone_rx, d, n < sizeof(phone_rx) ? n : sizeof(phone_rx));
        phone_rx_len = (int)n;
        return;
    }
    /* VESC frames it: 0x02, len, 36 (COMM_CUSTOM_APP_DATA), data, crc, 0x03 */
    uint8_t p[300];
    p[0] = 36;
    memcpy(&p[1], d, n);
    uint16_t c = crc16(p, n + 1);
    int t = to_lcm_tail;
#define PUT(x) (to_lcm[t++ % (int)sizeof(to_lcm)] = (uint8_t)(x))
    PUT(0x02);
    PUT(n + 1);
    for (unsigned i = 0; i < n + 1; i++)
        PUT(p[i]);
    PUT(c >> 8);
    PUT(c & 0xFF);
    PUT(0x03);
#undef PUT
    to_lcm_tail = t;
}
static vesc_c_if iface = {send_app_data};
vesc_c_if *VESC_IF = &iface;

/* the LCM's UART packets arrive at the package */
static uint8_t from_lcm[64];
static int from_lcm_n;
void bl_uart_put(uint32_t b)
{
    if (from_lcm_n == 0 && b != 0x02)
        return;
    from_lcm[from_lcm_n++] = (uint8_t)b;
    if (from_lcm_n >= 2 && from_lcm_n == from_lcm[1] + 5)
    {
        int len = from_lcm[1];
        from_lcm_n = 0;
        assert(crc16(&from_lcm[2], (uint32_t)len) == ((from_lcm[len + 2] << 8) | from_lcm[len + 3]));
        /* 36, 101, 128, sub... -> on_command_received(101, 128, ...) */
        assert(from_lcm[2] == 36 && from_lcm[3] == 101 && from_lcm[4] == 128);
        reply_to = TO_LCM;
        lcm_fw_command(&fw, &st, &tm, &from_lcm[5], len - 3);
    }
}

int bl_uart_get(void)
{
    if (to_lcm_head == to_lcm_tail)
        return -1;
    return to_lcm[to_lcm_head++ % (int)sizeof(to_lcm)];
}

/* ---------------------------------------------------------------- phone */

static const uint8_t *img;
static uint32_t img_size;

static void phone_cmd(const uint8_t *d, int n)
{
    /* d starts after 101, 128 */
    reply_to = TO_PHONE;
    phone_rx_len = 0;
    lcm_fw_command(&fw, &st, &tm, d, n);
}

static void phone_start(void)
{
    uint8_t b[7] = {LCM_FW_START};
    b[1] = img_size >> 24, b[2] = img_size >> 16, b[3] = img_size >> 8, b[4] = img_size;
    uint16_t c = crc16(img, img_size);
    b[5] = c >> 8, b[6] = c & 0xFF;
    phone_cmd(b, 7);
    assert(phone_rx_len == 13 && phone_rx[0] == 101 && phone_rx[1] == 128 && phone_rx[2] == LCM_FW_STATUS);
}

static void phone_send_page(uint32_t off)
{
    if (off >= img_size)
        return;
    uint8_t b[5 + PAGE] = {LCM_FW_DATA};
    b[1] = off >> 24, b[2] = off >> 16, b[3] = off >> 8, b[4] = off;
    memcpy(&b[5], img + off, PAGE);
    phone_cmd(b, 5 + PAGE);
    pages_sent++;
}

/* every 40 ms: STATUS, then the wanted page and the next one */
static void phone_tick(void)
{
    if (!phone_on || (phone_stop_ms >= 0 && sim_ms >= phone_stop_ms) || sim_ms % 40)
        return;
    uint8_t s = LCM_FW_STATUS;
    phone_cmd(&s, 1);
    assert(phone_rx_len == 13);
    uint32_t want = ((uint32_t)phone_rx[5] << 24) | (phone_rx[6] << 16) | (phone_rx[7] << 8) | phone_rx[8];
    if (phone_rx[3] == LCM_FW_STAGED || phone_rx[3] == LCM_FW_SENDING)
    {
        phone_send_page(want);
        phone_send_page(want + PAGE);
    }
}

/* ---------------------------------------------------- the rest of the LCM */

int bl_tick_ms(void)
{
    if (++sim_calls % 4)
        return 0;
    sim_ms++;
    tm.now += SYSTEM_TICK_RATE_HZ / 1000;
    phone_tick();
    if (sim_ms > 3600000)
    {
        printf("FAIL: an hour of simulated time\n");
        exit(1);
    }
    return 1;
}
int bl_button_pressed(void) { return 0; }
void bl_power_on(void) { sim_power = 1; }
void bl_power_off(void)
{
    sim_power = 0;
    longjmp(sim_exit, SIM_POWER_OFF);
}
void bl_hw_init(void) {}

void bl_flash_erase(uint32_t addr)
{
    if (sim_cut_at >= 0 && sim_flash_ops >= sim_cut_at)
        longjmp(sim_exit, SIM_POWER_CUT);
    sim_flash_ops++;
    assert(addr >= APP_BASE && addr < 0x08003F80u && addr % PAGE == 0);
    memset(&sim_flash[addr - SIM_FLASH_BASE], 0xFF, PAGE);
}
void bl_flash_write16(uint32_t addr, uint32_t value)
{
    if (sim_cut_at >= 0 && sim_flash_ops >= sim_cut_at)
        longjmp(sim_exit, SIM_POWER_CUT);
    sim_flash_ops++;
    assert(addr >= APP_BASE && addr < 0x08003F80u);
    uint8_t *p = &sim_flash[addr - SIM_FLASH_BASE];
    assert(p[0] == 0xFF && p[1] == 0xFF);
    p[0] = (uint8_t)value, p[1] = (uint8_t)(value >> 8);
}
uint32_t bl_rd32(uint32_t a)
{
    const uint8_t *p = &sim_flash[a - SIM_FLASH_BASE];
    return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24);
}
uint32_t bl_rd8(uint32_t a) { return sim_flash[a - SIM_FLASH_BASE]; }
const void *bl_ptr(uint32_t a) { return &sim_flash[a - SIM_FLASH_BASE]; }
void bl_start_app(void) { longjmp(sim_exit, SIM_APP_STARTED); }

/* ---------------------------------------------------------------- tests */

static uint8_t img_a[MAXIMG], img_b[MAXIMG];
static uint32_t size_a, size_b;

static void make_image(uint8_t *im, uint32_t *size, uint32_t len, uint32_t seed)
{
    memset(im, 0xFF, MAXIMG);
    uint32_t sp = 0x20000800u, pc = APP_BASE + 0x101u;
    memcpy(im, &sp, 4);
    memcpy(im + 4, &pc, 4);
    for (uint32_t i = 8; i < len; i++)
        im[i] = (uint8_t)((seed = seed * 1103515245u + 12345u) >> 16);
    *size = (len + PAGE - 1) / PAGE * PAGE;
}

static void reset_links(void)
{
    sim_ms = sim_calls = 0;
    to_lcm_head = to_lcm_tail = from_lcm_n = 0;
    pages_sent = 0;
    phone_stop_ms = -1;
}

/* The ALCM app: hands over when the VP poll reply carries the flag. */
static int app_sees_flag_and_updates(void)
{
    reset_links();
    int r = setjmp(sim_exit);
    if (r)
        return r;
    if (!lcm_fw_update_flag(&fw, &st, &tm))
        return -1;
    bl_updater(1);
    bl_start_app();
}

static int power_up(void)
{
    reset_links();
    int r = setjmp(sim_exit);
    if (r)
        return r;
    if (!bl_app_valid())
        bl_updater(0);
    bl_start_app();
}

static int app_is(const uint8_t *im, uint32_t size)
{
    return memcmp(&sim_flash[APP_OFF], im, size) == 0;
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    make_image(img_a, &size_a, 9000, 1);
    make_image(img_b, &size_b, MAXIMG, 2);

    /* 1. normal update A -> B */
    memset(sim_flash, 0xFF, sizeof sim_flash);
    memcpy(&sim_flash[APP_OFF], img_a, size_a);
    lcm_fw_init(&fw);
    st.state = STATE_READY;
    img = img_b, img_size = size_b;
    phone_start();
    assert(fw.state == LCM_FW_STAGED && lcm_fw_update_flag(&fw, &st, &tm));
    st.state = STATE_RUNNING;
    assert(!lcm_fw_update_flag(&fw, &st, &tm)); /* never while running */
    st.state = STATE_READY;
    assert(app_sees_flag_and_updates() == SIM_APP_STARTED);
    assert(app_is(img_b, size_b) && fw.state == LCM_FW_OK && !lcm_fw_update_flag(&fw, &st, &tm));
    printf("ok  phone -> package -> bootloader: %u bytes in %ld ms simulated, %d pages sent\n", size_b,
           sim_ms, pages_sent);

    /* 2. START refused while running */
    st.state = STATE_RUNNING;
    phone_start();
    assert(fw.state == LCM_FW_FAILED && fw.error == LCM_FW_ERR_RUNNING);
    st.state = STATE_READY;
    printf("ok  START refused while the board is running\n");

    /* 3. no bootloader on the LCM: the flag is never answered -> times out */
    lcm_fw_init(&fw);
    phone_start();
    tm.now += 16 * SYSTEM_TICK_RATE_HZ;
    assert(!lcm_fw_update_flag(&fw, &st, &tm) && fw.state == LCM_FW_FAILED &&
           fw.error == LCM_FW_ERR_NO_BOOTLOADER);
    printf("ok  LCM without bootloader: NO_BOOTLOADER after 15 s\n");

    /* 4. phone goes away mid-update: no partial app; at the next power-up the
     * bootloader holds the power and waits; a new phone session finishes */
    memset(sim_flash, 0xFF, sizeof sim_flash);
    memcpy(&sim_flash[APP_OFF], img_a, size_a);
    lcm_fw_init(&fw);
    img = img_b, img_size = size_b;
    phone_start();
    {
        reset_links();
        phone_stop_ms = 1500;
        int r = setjmp(sim_exit);
        if (r == 0)
        {
            assert(lcm_fw_update_flag(&fw, &st, &tm));
            bl_updater(1);
            bl_start_app();
        }
        assert(r == SIM_POWER_OFF); /* nothing for 5 min: power released */
    }
    assert(!bl_app_valid());
    /* power-up: package restarted too (fresh state), phone starts again */
    lcm_fw_init(&fw);
    phone_start();
    assert(power_up() == SIM_APP_STARTED && app_is(img_b, size_b) && fw.state == LCM_FW_OK);
    printf("ok  phone lost mid-update: no partial app, next power-up finishes it\n");

    printf("all relay tests passed\n");
    return 0;
}
