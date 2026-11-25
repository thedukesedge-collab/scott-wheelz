/*
 * Refloat Glow: register-level equivalents of a few vendor library
 * calls, used to make room for the LCM bootloader. Each writes exactly
 * what the vendor function writes for the given arguments.
 *
 * GPL-3.0-or-later.
 */
#ifndef HW_REGS_H
#define HW_REGS_H

#include "hk32f030m.h"

/* = NVIC_Init() with NVIC_IRQChannelCmd = ENABLE. Note the vendor shifts
 * the priority by 6 (not by 8 - __NVIC_PRIO_BITS like CMSIS), so this does
 * too, to keep the exact same priority values. */
static inline void hw_nvic_enable(uint32_t irq, uint32_t priority)
{
    uint32_t shift = (irq & 0x03U) * 8U;

    NVIC->IP[irq >> 2] = (NVIC->IP[irq >> 2] & ~(0xFFUL << shift)) | (((priority << 6) & 0xFFU) << shift);
    NVIC->ISER[0] = 1UL << (irq & 0x1FU);
}

/* = GPIO_StructInit() + GPIO_Init() for one pin (pin number 0-7, not the
 * GPIO_Pin_x mask), in one shared function (power_hw.c) instead of the
 * vendor's 16-pin loop and a filled struct at every call. cfg = HW_GPIO(...)
 * below. Same registers, same values, same order: speed and output type only
 * for output / alternate function, then mode, pull, Schmitt trigger (the
 * vendor's GPIO_Schmit_Disable, the zero default, sets the IOSR bit). */
#define HW_GPIO(mode, otype, speed, pupd, schmitt)                                       \
    ((uint32_t)(mode) | ((uint32_t)(otype) << 2) | ((uint32_t)(speed) << 3) |             \
     ((uint32_t)(pupd) << 5) | ((uint32_t)(schmitt) << 7))
/* push-pull output, 10 MHz, no pull (most pins) */
#define HW_GPIO_OUT HW_GPIO(GPIO_Mode_OUT, GPIO_OType_PP, GPIO_Speed_10MHz, GPIO_PuPd_NOPULL, 0)
void hw_gpio_pin(GPIO_TypeDef *gpio, uint32_t pin, uint32_t cfg);

#endif
