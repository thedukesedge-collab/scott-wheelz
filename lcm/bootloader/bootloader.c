/*
 * LCM bootloader for the HK32F030MF4P6 light control module
 * (Floatwheel ADV100 boards: XRV, GTV, PintV) running ALCM VP.
 *
 * Part of Re-Float Rink-Mixed. GPL-3.0-or-later.
 *
 * Flash map (16 KB, 128-byte pages):
 *   0x08000000 - 0x080003FF  this bootloader (1 KB, never rewritten by it)
 *   0x08000400 - 0x08003F7F  ALCM application (vector table at 0x08000400,
 *                            selected with FLASH->INT_VEC_OFFSET = 0x400)
 *   0x08003F80 - 0x08003FFF  last page, holds the battery LUT block at
 *                            0x08003FC0 (tools/battery_lut_patch); never
 *                            written by updates, so the LUT is kept
 *
 * Normal power-up: if the application looks valid, jump to it at once, with
 * nothing touched, so the app starts exactly as it did without us.
 *
 * Update: on the VP poll-reply flag (LCM_VP_FLAG_UPDATE) the app calls the
 * entry point stored in vector slot 7 (0x0800001C). We then pull the image
 * from the VP package over the existing VESC UART link, a page at a time:
 *
 *   LCM -> VESC  36 (COMM_CUSTOM_APP_DATA), 101, 127 (COMMAND_LCM_FW),
 *                0x10 (LCM_FW_REQ), offset (u32 BE)
 *   VESC -> LCM  36, 101, 127, 0x10, offset (u32), image size (u32),
 *                image CRC16 (u16), then 128 data bytes (the image is
 *                padded with 0xFF to whole 128-byte pages);
 *                size 0 = nothing staged (yet), ask again later
 *   LCM -> VESC  36, 101, 127, 0x11 (LCM_FW_DONE), status:
 *                0 = written and verified, 1 = CRC mismatch (starts over),
 *                2 = nothing came, back to the unchanged app
 *
 * Pages are requested 128, 256, ... and page 0 (the app's vector table)
 * last; page 0 is erased as soon as the first chunk arrives. So until the
 * whole image is in and verified there is no valid app, and an update that
 * is cut short (power loss) just ends up back here on the next power-up,
 * waiting for the image again. While waiting, the power latch is held on;
 * after ~5 minutes with no image it is released (board off).
 *
 * The image CRC is CRC16-CCITT (XMODEM), the same as the VESC packet CRC.
 * No interrupts are used.
 */
#include "hk32f030m.h"

#define APP_BASE 0x08000400u
#define FLASH_END 0x08003F80u /* last page = battery LUT, never written */
#define PAGE_SIZE 128u
#define MAX_IMAGE (FLASH_END - APP_BASE)

#define COMM_CUSTOM_APP_DATA 36u
#define LCM_PACKAGE_ID 101u
#define COMMAND_LCM_FW 127u
#define LCM_FW_REQ 0x10u
#define LCM_FW_DONE 0x11u

#define HDR 14u /* 36,101,127,sub, offset(4), size(4), crc(2) */
#define RX_MAX (HDR + PAGE_SIZE)

#define REQUEST_TIMEOUT_MS 300u
#define GIVE_UP_TRIES 66u     /* ~20 s without an image when coming from the app */
#define POWER_OFF_TRIES 1000u /* ~5 min without an image: power off */

extern uint32_t _estack;

void Reset_Handler(void);
void Update_Entry(void);

/* Only the core vectors: interrupts are never enabled here. Slot 7
 * (reserved on Cortex-M0) holds the update entry the app calls. */
__attribute__((section(".isr_vector"), used)) void (*const vectors[8])(void) = {
    (void (*)(void))(&_estack), Reset_Handler, 0, 0, 0, 0, 0, Update_Entry,
};

/* ---------------------------------------------------------------- app */

static int app_valid(void)
{
    uint32_t sp = *(volatile uint32_t *)APP_BASE;
    uint32_t pc = *(volatile uint32_t *)(APP_BASE + 4u);

    return ((sp - 0x20000001u) < 0x800u) && ((pc & 1u) != 0u) && (pc - APP_BASE < MAX_IMAGE);
}

static void __attribute__((noreturn)) jump_to_app(void)
{
    uint32_t sp = *(volatile uint32_t *)APP_BASE;
    uint32_t pc = *(volatile uint32_t *)(APP_BASE + 4u);

    __asm volatile("msr msp, %0\n bx %1\n" : : "r"(sp), "r"(pc));
    __builtin_unreachable();
}

/* --------------------------------------------------------------- UART */

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
            {
                crc ^= 0x1021u;
            }
        }
    }
    return (uint16_t)crc;
}

static void uart_put(uint32_t b)
{
    while ((USART1->ISR & USART_ISR_TXE) == 0u)
    {
    }
    USART1->TDR = b;
}

/* Sends a FW packet: 0x02, len, 36, 101, 127, sub, value(4), crc, 0x03.
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
    uint32_t crc = crc16(p, len);

    uart_put(0x02);
    uart_put(len);
    for (uint32_t i = 0; i < len; i++)
    {
        uart_put(p[i]);
    }
    uart_put(crc >> 8);
    uart_put(crc & 0xFFu);
    uart_put(0x03);
}

/* Next received byte, or -1 once *budget_ms runs out. */
static int uart_get(uint32_t *budget_ms)
{
    for (;;)
    {
        if (USART1->ISR & USART_ISR_ORE)
        {
            USART1->ICR = USART_ICR_ORECF;
        }
        if (USART1->ISR & USART_ISR_RXNE)
        {
            return (int)(USART1->RDR & 0xFFu);
        }
        if ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) && (*budget_ms)-- == 0u)
        {
            return -1;
        }
    }
}

/* Waits for a valid short VESC packet; returns its payload length, or 0 on
 * timeout. buf receives payload, crc (2) and end byte. */
static uint32_t receive_packet(uint8_t *buf)
{
    uint32_t budget = REQUEST_TIMEOUT_MS;

    for (;;)
    {
        int b = uart_get(&budget);
        if (b < 0)
        {
            return 0;
        }
        if (b != 0x02)
        {
            continue;
        }
        int len = uart_get(&budget);
        if (len <= 0 || len > (int)RX_MAX)
        {
            continue;
        }
        for (int i = 0; i < len + 3; i++)
        {
            b = uart_get(&budget);
            if (b < 0)
            {
                return 0;
            }
            buf[i] = (uint8_t)b;
        }
        if (buf[len + 2] == 0x03u && crc16(buf, (uint32_t)len) == ((buf[len] << 8) | buf[len + 1]))
        {
            return (uint32_t)len;
        }
    }
}

/* -------------------------------------------------------------- flash */

static void flash_wait(void)
{
    while (FLASH->SR & FLASH_SR_BSY)
    {
    }
    FLASH->SR = FLASH_SR_EOP;
}

static void flash_erase(uint32_t addr)
{
    FLASH->CR = FLASH_CR_PER;
    FLASH->AR = addr;
    FLASH->CR = FLASH_CR_PER | FLASH_CR_STRT;
    flash_wait();
    FLASH->CR = 0;
}

/* ------------------------------------------------------------ updater */

static uint32_t get_u32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static void __attribute__((noreturn)) updater(int from_app)
{
    uint8_t rx[RX_MAX + 3];
    uint32_t offset = PAGE_SIZE, size = 0, crc = 0, tries = 0;
    int erased = 0;

    /* HSI 32 MHz with the factory trim, as the app's SystemInit sets it
     * (no-op when coming from the app). */
    uint32_t trim = *(volatile uint32_t *)0x1FFFF820u;
    RCC->CR |= 1u;
    RCC->CFGR &= 0xF8FFB81Cu; /* SW = HSI, HCLK = PCLK = SYSCLK */
    if (((trim >> 16) & trim & 0xFFFFu) == 0u)
    {
        RCC->CR |= ((trim & 0x1Fu) << 3) | (((trim >> 5) & 0x3Fu) << 8);
    }
    RCC->CFGR4 = (RCC->CFGR4 & ~RCC_RCC_CFGR4_FLITFCLK_PRE) | (7u << RCC_RCC_CFGR4_FLITFCLK_PRE_Pos);
    FLASH->ACR = (FLASH->ACR & ~7u) | 2u; /* 2 wait states */
    FLASH->INT_VEC_OFFSET = 0;

    /* GPIO: PA2 power latch on, PD1/PD6 USART1 TX/RX (AF1). Coming from
     * the app these are already set up (PA2 is config-locked there, which
     * only blocks the MODER write). */
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN | RCC_AHBENR_GPIODEN;
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
    GPIOA->BSRR = 1u << 2;
    GPIOA->MODER = (GPIOA->MODER & ~(3u << 4)) | (1u << 4);
    GPIOD->MODER = (GPIOD->MODER & ~((3u << 2) | (3u << 12))) | (2u << 2) | (2u << 12);
    GPIOD->AFR[0] = (GPIOD->AFR[0] & ~((0xFu << 4) | (0xFu << 24))) | (1u << 4) | (1u << 24);

    USART1->CR1 = 0;
    USART1->BRR = 278u; /* 32 MHz / 115200 */
    USART1->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;

    SysTick->LOAD = 32000u - 1u; /* 1 ms */
    SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;

    FLASH->KEYR = FLASH_FKEY1;
    FLASH->KEYR = FLASH_FKEY2;

    for (;;)
    {
        if (size != 0u && offset >= size)
        {
            offset = 0; /* page 0 goes last */
        }
        send_fw(LCM_FW_REQ, offset, 8);
        uint32_t len = receive_packet(rx);
        uint32_t r_size = get_u32(&rx[8]);
        uint32_t r_crc = (uint32_t)(rx[12] << 8) | rx[13];
        if (len < HDR || get_u32(rx) != ((COMM_CUSTOM_APP_DATA << 24) | (LCM_PACKAGE_ID << 16) |
                                         (COMMAND_LCM_FW << 8) | LCM_FW_REQ) ||
            r_size == 0u || r_size > MAX_IMAGE || (r_size % PAGE_SIZE) != 0u)
        {
            /* timeout, other packet or nothing staged: ask again */
            ++tries;
            if (from_app && !erased && tries >= GIVE_UP_TRIES)
            {
                send_fw(LCM_FW_DONE, 2u << 24, 5);
                break;
            }
            if (tries >= POWER_OFF_TRIES)
            {
                GPIOA->BRR = 1u << 2; /* nothing for 5 min: release the power latch */
            }
        }
        else if (r_size != size || r_crc != crc)
        {
            size = r_size; /* (new) image: start over */
            crc = r_crc;
            offset = PAGE_SIZE;
        }
        else if (get_u32(&rx[4]) == offset && len == RX_MAX)
        {
            uint32_t addr = APP_BASE + offset;

            tries = 0;
            if (!erased)
            {
                flash_erase(APP_BASE); /* no valid app until we're done */
                erased = 1;
            }
            flash_erase(addr);
            FLASH->CR = FLASH_CR_PG;
            for (uint32_t i = 0; i < PAGE_SIZE; i += 2u)
            {
                *(volatile uint16_t *)(addr + i) = (uint16_t)(rx[HDR + i] | (rx[HDR + i + 1u] << 8));
                flash_wait();
            }
            FLASH->CR = 0;

            if (offset == 0u)
            {
                if (crc16((const uint8_t *)APP_BASE, size) == crc && app_valid())
                {
                    send_fw(LCM_FW_DONE, 0, 5);
                    break;
                }
                send_fw(LCM_FW_DONE, 1u << 24, 5);
                size = 0; /* re-read size/CRC and start over */
            }
            offset += PAGE_SIZE;
        }
    }

    /* hand over in a clean state for the app's own init */
    while ((USART1->ISR & USART_ISR_TC) == 0u)
    {
    }
    FLASH->CR = FLASH_CR_LOCK;
    SysTick->CTRL = 0;
    NVIC->ICPR[0] = 0xFFFFFFFFu;
    __asm volatile("cpsie i");
    jump_to_app();
}

void __attribute__((used, noreturn)) update_from_app(void)
{
    NVIC->ICER[0] = 0xFFFFFFFFu;
    updater(1);
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

void Reset_Handler(void)
{
    if (app_valid())
    {
        jump_to_app();
    }
    updater(0);
}
