/*
 * Host tests for the LCM bootloader (bootloader.c built with -DBL_HOST).
 *
 * A simulated VP package answers the bootloader's page requests over a
 * simulated UART. The tests cover a normal update, a cold boot with no app,
 * "nothing staged" from the app, a power cut at EVERY flash operation of an
 * update (the board must then run either the complete old app, the complete
 * new app, or stay in the bootloader - never a partial app), corrupted
 * pages, a wrong-address image, the button power-off and the idle power-off,
 * and that only the app area is ever erased or written.
 */
#include <assert.h>
#include <string.h>

#include "bl_host.h"

#define APP_BASE 0x08000400u
#define APP_OFF (APP_BASE - SIM_FLASH_BASE)
#define FLASH_END_OFF (0x08003F80u - SIM_FLASH_BASE)
#define PAGE 128u

uint8_t sim_flash[SIM_FLASH_SIZE];
jmp_buf sim_exit;
int sim_power;
long sim_flash_ops, sim_cut_at = -1;
int sim_bad_write;

static long sim_ms, sim_calls;
static int sim_button;      /* level now */
static long sim_button_from = -1, sim_button_to = -1; /* pressed in [from, to) ms */

/* ---------------------------------------------------------------- UART */

static uint8_t to_lcm[4096];
static int to_lcm_head, to_lcm_tail;
static long to_lcm_ready_ms; /* reply latency */
static uint8_t from_lcm[64];
static int from_lcm_n;

/* package model */
static const uint8_t *pkg_image; /* NULL = nothing staged */
static uint32_t pkg_size;
static uint16_t pkg_crc;
static int pkg_present = 1;
static int pkg_corrupt_offset = -1, pkg_corrupt_times; /* flip a bit in that page */
static int pkg_done_status = -1, pkg_requests;

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

static void send_to_lcm(const uint8_t *payload, int len)
{
    uint16_t c = crc16(payload, (uint32_t)len);
    uint8_t *q = to_lcm;
    int t = to_lcm_tail;
#define PUT(x) (q[t++ % (int)sizeof(to_lcm)] = (uint8_t)(x))
    PUT(0x02);
    PUT(len);
    for (int i = 0; i < len; i++)
        PUT(payload[i]);
    PUT(c >> 8);
    PUT(c & 0xFF);
    PUT(0x03);
#undef PUT
    to_lcm_tail = t;
    to_lcm_ready_ms = sim_ms + 2;
}

static void put_u32(uint8_t *p, uint32_t v)
{
    p[0] = v >> 24, p[1] = v >> 16, p[2] = v >> 8, p[3] = v;
}

static void package_handle(const uint8_t *p, int len)
{
    if (!pkg_present || len < 5 || p[0] != 36 || p[1] != 101 || p[2] != 127)
        return;
    if (p[3] == 0x11)
    {
        pkg_done_status = p[4];
        return;
    }
    if (p[3] != 0x10 || len != 8)
        return;
    pkg_requests++;
    uint32_t off = ((uint32_t)p[4] << 24) | (p[5] << 16) | (p[6] << 8) | p[7];
    uint8_t r[14 + PAGE];
    r[0] = 36, r[1] = 101, r[2] = 127, r[3] = 0x10;
    put_u32(&r[4], off);
    if (!pkg_image || off >= pkg_size)
    {
        put_u32(&r[8], pkg_image ? pkg_size : 0);
        r[12] = pkg_crc >> 8, r[13] = pkg_crc & 0xFF;
        send_to_lcm(r, 14);
        return;
    }
    put_u32(&r[8], pkg_size);
    r[12] = pkg_crc >> 8, r[13] = pkg_crc & 0xFF;
    memcpy(&r[14], pkg_image + off, PAGE);
    if ((int)off == pkg_corrupt_offset && pkg_corrupt_times != 0)
    {
        r[14 + 5] ^= 0x10;
        if (pkg_corrupt_times > 0)
            pkg_corrupt_times--;
    }
    send_to_lcm(r, 14 + PAGE);
}

void bl_uart_put(uint32_t b)
{
    /* collect one LCM packet: 0x02 len payload crc crc 0x03 */
    if (from_lcm_n == 0 && b != 0x02)
        return;
    from_lcm[from_lcm_n++] = (uint8_t)b;
    if (from_lcm_n >= 2 && from_lcm_n == from_lcm[1] + 5)
    {
        int len = from_lcm[1];
        assert(from_lcm[len + 4] == 0x03);
        assert(crc16(&from_lcm[2], (uint32_t)len) == ((from_lcm[len + 2] << 8) | from_lcm[len + 3]));
        package_handle(&from_lcm[2], len);
        from_lcm_n = 0;
    }
}

int bl_uart_get(void)
{
    if (to_lcm_head == to_lcm_tail || sim_ms < to_lcm_ready_ms)
        return -1;
    return to_lcm[to_lcm_head++ % (int)sizeof(to_lcm)];
}

/* ------------------------------------------------------- time / button */

int bl_tick_ms(void)
{
    /* about 4 UART bytes per ms (real link: ~11.5) */
    if (++sim_calls % 4)
        return 0;
    sim_ms++;
    sim_button = sim_button_from >= 0 && sim_ms >= sim_button_from &&
                 (sim_button_to < 0 || sim_ms < sim_button_to);
    if (sim_ms > 3600000)
    {
        printf("FAIL: bootloader ran for an hour of simulated time\n");
        exit(1);
    }
    return 1;
}

int bl_button_pressed(void)
{
    return sim_button;
}

/* --------------------------------------------------------- power, flash */

void bl_power_on(void)
{
    sim_power = 1;
}

void bl_power_off(void)
{
    sim_power = 0;
    longjmp(sim_exit, SIM_POWER_OFF);
}

void bl_hw_init(void) {}

static void flash_op(void)
{
    if (sim_cut_at >= 0 && sim_flash_ops >= sim_cut_at)
        longjmp(sim_exit, SIM_POWER_CUT);
    sim_flash_ops++;
}

static int in_app(uint32_t addr)
{
    return addr >= APP_BASE && addr < 0x08003F80u;
}

void bl_flash_erase(uint32_t addr)
{
    flash_op();
    if (!in_app(addr) || (addr % PAGE) != 0)
    {
        sim_bad_write = 1;
        return;
    }
    memset(&sim_flash[addr - SIM_FLASH_BASE], 0xFF, PAGE);
}

void bl_flash_write16(uint32_t addr, uint32_t value)
{
    flash_op();
    if (!in_app(addr) || (addr & 1))
    {
        sim_bad_write = 1;
        return;
    }
    uint8_t *p = &sim_flash[addr - SIM_FLASH_BASE];
    if (p[0] != 0xFF || p[1] != 0xFF)
        sim_bad_write = 1; /* programming a non-erased halfword */
    p[0] &= (uint8_t)value;
    p[1] &= (uint8_t)(value >> 8);
}

uint32_t bl_rd32(uint32_t addr)
{
    const uint8_t *p = &sim_flash[addr - SIM_FLASH_BASE];
    return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24);
}

uint32_t bl_rd8(uint32_t addr)
{
    return sim_flash[addr - SIM_FLASH_BASE];
}

const void *bl_ptr(uint32_t addr)
{
    return &sim_flash[addr - SIM_FLASH_BASE];
}

void bl_start_app(void)
{
    longjmp(sim_exit, SIM_APP_STARTED);
}

/* -------------------------------------------------------------- helpers */

static uint8_t img_a[0x3B80], img_b[0x3B80], img_bad[0x3B80];
static uint32_t size_a, size_b, size_bad;

/* An app image: valid SP/reset vector at the start, pseudo-random body,
 * padded with 0xFF to whole pages. */
static void make_image(uint8_t *img, uint32_t *size, uint32_t len, uint32_t seed, uint32_t base)
{
    memset(img, 0xFF, 0x3B80);
    uint32_t sp = 0x20000800u, pc = base + 0x101u;
    memcpy(img, &sp, 4);
    memcpy(img + 4, &pc, 4);
    for (uint32_t i = 8; i < len; i++)
    {
        seed = seed * 1103515245u + 12345u;
        img[i] = (uint8_t)(seed >> 16);
    }
    *size = (len + PAGE - 1) / PAGE * PAGE;
}

static void flash_reset(void)
{
    memset(sim_flash, 0xFF, sizeof(sim_flash));
    /* recognisable bootloader and battery LUT contents */
    for (uint32_t i = 0; i < APP_OFF; i++)
        sim_flash[i] = (uint8_t)(i * 7);
    for (uint32_t i = FLASH_END_OFF; i < SIM_FLASH_SIZE; i++)
        sim_flash[i] = (uint8_t)(i * 13);
}

static void install(const uint8_t *img, uint32_t size)
{
    memcpy(&sim_flash[APP_OFF], img, size);
}

static int app_is(const uint8_t *img, uint32_t size)
{
    return memcmp(&sim_flash[APP_OFF], img, size) == 0;
}

static void check_protected_areas(void)
{
    for (uint32_t i = 0; i < APP_OFF; i++)
        assert(sim_flash[i] == (uint8_t)(i * 7));
    for (uint32_t i = FLASH_END_OFF; i < SIM_FLASH_SIZE; i++)
        assert(sim_flash[i] == (uint8_t)(i * 13));
    assert(!sim_bad_write);
}

static void stage(const uint8_t *img, uint32_t size)
{
    pkg_image = img;
    pkg_size = size;
    pkg_crc = img ? crc16(img, size) : 0;
}

static void sim_reset_runtime(void)
{
    sim_ms = sim_calls = 0;
    sim_button = 0;
    sim_button_from = sim_button_to = -1;
    to_lcm_head = to_lcm_tail = 0;
    from_lcm_n = 0;
    pkg_done_status = -1;
    pkg_requests = 0;
    sim_power = 0;
}

/* Power-up through the reset path: Reset_Handler does exactly this. */
static int cold_boot(void)
{
    sim_reset_runtime();
    int r = setjmp(sim_exit);
    if (r)
        return r;
    if (!bl_app_valid())
        bl_updater(0);
    bl_start_app();
}

/* The app calling Update_Entry. */
static int from_app(void)
{
    sim_reset_runtime();
    sim_power = 1;
    int r = setjmp(sim_exit);
    if (r)
        return r;
    bl_updater(1);
    bl_start_app();
}

/* ---------------------------------------------------------------- tests */

static void test_cold_boot_valid_app(void)
{
    flash_reset();
    install(img_a, size_a);
    sim_flash_ops = 0;
    assert(cold_boot() == SIM_APP_STARTED);
    assert(sim_flash_ops == 0 && pkg_requests == 0); /* nothing touched */
    assert(sim_power == 0);                         /* PA2 left to the app */
    printf("ok  cold boot with a valid app starts it, nothing touched\n");
}

static void test_update_from_app(void)
{
    flash_reset();
    install(img_a, size_a);
    stage(img_b, size_b);
    assert(from_app() == SIM_APP_STARTED);
    assert(app_is(img_b, size_b) && pkg_done_status == 0 && sim_power == 1);
    check_protected_areas();
    printf("ok  full-size update from the app (%u bytes, %ld ms simulated)\n", size_b, sim_ms);
}

static void test_cold_boot_no_app(void)
{
    flash_reset();
    stage(img_a, size_a);
    assert(cold_boot() == SIM_APP_STARTED);
    assert(app_is(img_a, size_a) && pkg_done_status == 0 && sim_power == 1);
    check_protected_areas();
    printf("ok  cold boot with no app: power held, image pulled, app started\n");
}

static void test_nothing_staged(void)
{
    flash_reset();
    install(img_a, size_a);
    stage(NULL, 0);
    sim_flash_ops = 0;
    assert(from_app() == SIM_APP_STARTED);
    assert(pkg_done_status == 2 && sim_flash_ops == 0 && app_is(img_a, size_a));
    assert(sim_ms >= 20000 && sim_ms < 21000);
    printf("ok  from the app with nothing staged: back to the old app after 20 s\n");
}

/* Power cut at every flash operation of an update A -> B, then power-ups
 * until done. After the cut, a valid-looking app must be exactly A or B. */
static void test_power_cut_everywhere(const uint8_t *a, uint32_t sa, const uint8_t *b, uint32_t sb)
{
    flash_reset();
    install(a, sa);
    stage(b, sb);
    sim_flash_ops = 0;
    sim_cut_at = -1;
    assert(from_app() == SIM_APP_STARTED);
    long total = sim_flash_ops;

    for (long cut = 0; cut <= total; cut++)
    {
        flash_reset();
        install(a, sa);
        stage(b, sb);
        sim_flash_ops = 0;
        sim_cut_at = cut;
        int r = from_app();
        sim_cut_at = -1;
        if (r == SIM_APP_STARTED)
        {
            assert(cut == total && app_is(b, sb));
            continue;
        }
        assert(r == SIM_POWER_CUT);
        if (bl_app_valid())
        {
            /* a power-up would run it: it must be one of the two, whole */
            assert(app_is(a, sa) || app_is(b, sb));
        }
        /* power back on: either runs a whole app or finishes the update */
        r = cold_boot();
        assert(r == SIM_APP_STARTED);
        assert(app_is(a, sa) || app_is(b, sb));
        if (!app_is(b, sb))
        {
            assert(app_is(a, sa));
            /* old app still whole (cut before page 0 was erased): retry */
            assert(from_app() == SIM_APP_STARTED && app_is(b, sb));
        }
        check_protected_areas();
    }
    printf("ok  power cut at each of %ld flash operations: never a partial app\n", total + 1);
}

static void test_corrupt_page(void)
{
    flash_reset();
    install(img_a, size_a);
    stage(img_b, size_b);
    /* one bad page that the package then sends correctly: the image CRC
     * fails once, page 0 is erased again, the update starts over */
    pkg_corrupt_offset = 3 * PAGE;
    pkg_corrupt_times = 1;
    assert(from_app() == SIM_APP_STARTED && app_is(img_b, size_b));
    check_protected_areas();

    /* a page that is always bad: never a valid app; after 3 bad images no
     * more writing, and the board powers off after 5 idle minutes */
    flash_reset();
    install(img_a, size_a);
    pkg_corrupt_offset = 3 * PAGE;
    pkg_corrupt_times = -1;
    sim_flash_ops = 0;
    int r = from_app();
    assert(r == SIM_POWER_OFF && !bl_app_valid());
    assert(sim_flash_ops < 4 * (1 + 120 * 65)); /* about 3 images' worth of writes */
    pkg_corrupt_offset = -1;
    printf("ok  corrupted page: CRC caught, no app until a good image arrives\n");
}

static void test_wrong_address_image(void)
{
    flash_reset();
    install(img_a, size_a);
    stage(img_bad, size_bad); /* linked for 0x08000000: reset vector below the app */
    int r = from_app();
    assert(r == SIM_POWER_OFF && !bl_app_valid());
    check_protected_areas();
    printf("ok  image linked for the wrong address is never started\n");
}

static void test_button_power_off_2(void)
{
    flash_reset();
    stage(NULL, 0);
    sim_reset_runtime();
    int r;
    /* held from power-up for 10 s: no power-off while the first press lasts */
    sim_button = 1;
    sim_button_from = 0;
    sim_button_to = 10000;
    pkg_present = 1;
    if ((r = setjmp(sim_exit)) == 0)
    {
        /* after the release at 10 s, nothing else: idle power-off at 5 min */
        bl_updater(0);
    }
    assert(r == SIM_POWER_OFF && sim_ms >= 300000);

    /* release, then hold for 2 s: power off */
    sim_reset_runtime();
    sim_button = 1;
    sim_button_from = 0; /* power-up press */
    sim_button_to = 300;
    if ((r = setjmp(sim_exit)) == 0)
    {
        bl_updater(0);
    }
    /* first press released at 300 ms, never pressed again: idle off */
    assert(r == SIM_POWER_OFF);

    sim_reset_runtime();
    sim_button = 0;
    sim_button_from = 5000;
    sim_button_to = -1;
    if ((r = setjmp(sim_exit)) == 0)
    {
        bl_updater(0);
    }
    if (!(r == SIM_POWER_OFF && sim_ms >= 6990 && sim_ms < 7100)) printf("r=%d ms=%ld\n", r, sim_ms);
    assert(r == SIM_POWER_OFF && sim_ms >= 6990 && sim_ms < 7100);
    printf("ok  button: power-up press ignored, 2 s hold powers off; idle off at 5 min\n");
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    make_image(img_a, &size_a, 0x3B80 - 200, 1, APP_BASE);
    make_image(img_b, &size_b, 0x3B80, 2, APP_BASE);
    make_image(img_bad, &size_bad, 4000, 3, 0x08000000u);

    test_cold_boot_valid_app();
    test_update_from_app();
    test_cold_boot_no_app();
    test_nothing_staged();
    test_corrupt_page();
    test_wrong_address_image();
    test_button_power_off_2();

    /* power cut at every flash operation: small images both ways, then
     * full-size images */
    static uint8_t sa[0x3B80], sb[0x3B80];
    uint32_t na, nb;
    make_image(sa, &na, 9 * PAGE - 10, 11, APP_BASE);
    make_image(sb, &nb, 12 * PAGE, 12, APP_BASE);
    test_power_cut_everywhere(sa, na, sb, nb);
    test_power_cut_everywhere(sb, nb, sa, na); /* to a smaller image */
    test_power_cut_everywhere(img_a, size_a, img_b, size_b); /* full size */

    printf("all bootloader tests passed\n");
    return 0;
}
