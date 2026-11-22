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

#endif
