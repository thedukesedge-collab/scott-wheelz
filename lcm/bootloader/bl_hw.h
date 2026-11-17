/*
 * LCM bootloader: hardware access. On the LCM these are register-level
 * inline functions (no vendor library, no interrupts); for the host tests
 * (BL_HOST) they come from tests/bl_host.h, which simulates the flash,
 * the UART link to the package, the clock, the button and the power latch.
 *
 * Part of Re-Float Rink-Mixed. GPL-3.0-or-later.
 */
#pragma once

#ifdef BL_HOST
#include "bl_host.h"
#else

#include "hk32f030m.h"

extern uint32_t _estack;

static inline uint32_t bl_rd32(uint32_t addr)
{
    return *(volatile const uint32_t *)addr;
}

static inline uint32_t bl_rd8(uint32_t addr)
{
    return *(volatile const uint8_t *)addr;
}

static inline const void *bl_ptr(uint32_t addr)
{
    return (const void *)addr;
}

/* PA2 = PWR_EN, the power latch. Set the output level before the mode, so
 * the pin never drives low. Coming from the app PA2 is config-locked (the
 * MODER write is ignored) and already a high output. */
static inline void bl_power_on(void)
{
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN;
    GPIOA->BSRR = 1u << 2;
    GPIOA->MODER = (GPIOA->MODER & ~(3u << 4)) | (1u << 4);
}

/* Releases the power latch. The LCM loses power once the button is
 * released too, so this never returns. */
static inline void __attribute__((noreturn)) bl_power_off(void)
{
    GPIOA->BRR = 1u << 2;
    for (;;)
    {
    }
}

/* PC3, active low, pull-up (as the app sets it) */
static inline int bl_button_pressed(void)
{
    return (GPIOC->IDR & (1u << 3)) == 0u;
}

static inline void bl_hw_init(void)
{
    /* Flash wait states first (always safe), then HSI 32 MHz with the
     * factory trim, as the app's SystemInit sets it (no-op from the app). */
    FLASH->ACR = (FLASH->ACR & ~7u) | 2u;
    RCC->CFGR4 = (RCC->CFGR4 & ~RCC_RCC_CFGR4_FLITFCLK_PRE) | (7u << RCC_RCC_CFGR4_FLITFCLK_PRE_Pos);
    uint32_t trim = *(volatile uint32_t *)0x1FFFF820u;
    if (((trim >> 16) & trim & 0xFFFFu) == 0u)
    {
        RCC->CR = (RCC->CR & ~(RCC_CR_HSITRIM | RCC_CR_HSICAL)) | ((trim & 0x1Fu) << 3) |
                  (((trim >> 5) & 0x3Fu) << 8);
    }
    RCC->CR |= RCC_CR_HSION;
    while ((RCC->CR & RCC_CR_HSIRDY) == 0u)
    {
    }
    RCC->CFGR &= 0xF8FFB81Cu; /* SW = HSI, HCLK = PCLK = SYSCLK */
    FLASH->INT_VEC_OFFSET = 0;

    /* PC3 button input with pull-up; PD1/PD6 USART1 TX/RX (AF1) */
    RCC->AHBENR |= RCC_AHBENR_GPIOCEN | RCC_AHBENR_GPIODEN;
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
    GPIOC->MODER &= ~(3u << 6);
    GPIOC->PUPDR = (GPIOC->PUPDR & ~(3u << 6)) | (1u << 6);
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
}

/* 1 once per elapsed millisecond (reading COUNTFLAG clears it) */
static inline int bl_tick_ms(void)
{
    return (SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) != 0u;
}

static inline void bl_uart_put(uint32_t b)
{
    while ((USART1->ISR & USART_ISR_TXE) == 0u)
    {
    }
    USART1->TDR = b;
}

/* next received byte, or -1 */
static inline int bl_uart_get(void)
{
    if (USART1->ISR & USART_ISR_ORE)
    {
        USART1->ICR = USART_ICR_ORECF;
    }
    if (USART1->ISR & USART_ISR_RXNE)
    {
        return (int)(USART1->RDR & 0xFFu);
    }
    return -1;
}

static inline void bl_flash_wait(void)
{
    while (FLASH->SR & FLASH_SR_BSY)
    {
    }
    FLASH->SR = FLASH_SR_EOP;
}

/* Only the app region is ever erased or written: the bootloader itself and
 * the battery LUT page are out of reach whatever the caller asks. */
static inline int bl_in_app(uint32_t addr)
{
    return addr - 0x08000400u < 0x08003F80u - 0x08000400u;
}

static inline void bl_flash_erase(uint32_t addr)
{
    if (!bl_in_app(addr))
    {
        return;
    }
    FLASH->CR = FLASH_CR_PER;
    FLASH->AR = addr;
    FLASH->CR = FLASH_CR_PER | FLASH_CR_STRT;
    bl_flash_wait();
    FLASH->CR = 0;
}

static inline void bl_flash_write16(uint32_t addr, uint32_t value)
{
    if (!bl_in_app(addr))
    {
        return;
    }
    FLASH->CR = FLASH_CR_PG;
    *(volatile uint16_t *)addr = (uint16_t)value;
    bl_flash_wait();
    FLASH->CR = 0;
}

/* Hands over to the app in a clean, reset-like state for its own init
 * (it sets FLASH->INT_VEC_OFFSET = 0x400 in SystemInit). */
static inline void __attribute__((noreturn)) bl_start_app(void)
{
    while ((USART1->ISR & USART_ISR_TC) == 0u && (USART1->CR1 & USART_CR1_UE))
    {
    }
    USART1->CR1 = 0;
    FLASH->CR = FLASH_CR_LOCK;
    SysTick->CTRL = 0;
    NVIC->ICER[0] = 0xFFFFFFFFu;
    NVIC->ICPR[0] = 0xFFFFFFFFu;
    uint32_t sp = *(volatile uint32_t *)0x08000400u;
    uint32_t pc = *(volatile uint32_t *)0x08000404u;
    __asm volatile("cpsie i\n msr msp, %0\n bx %1\n" : : "r"(sp), "r"(pc));
    __builtin_unreachable();
}

#endif
