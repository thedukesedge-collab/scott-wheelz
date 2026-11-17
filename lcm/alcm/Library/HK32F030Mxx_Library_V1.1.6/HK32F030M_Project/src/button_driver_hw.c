/*
 * Copyright (c) 2024-2025, Mitchell White <mitchell.n.white@gmail.com>
 *
 * This file is part of Advanced LCM (ALCM) project.
 *
 * ALCM is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * ALCM is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along
 * with ALCM. If not, see <https://www.gnu.org/licenses/>.
 */

#include "hk32f030m.h"
#include "hw_regs.h"
#include "button_driver_hw.h"

#define BUTTON_PRESSED (((GPIOC->IDR >> 3) & 1) == 0)

void button_driver_hw_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOC, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);

    // Enable GPIO PC3
    GPIO_StructInit(&GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_InitStructure.GPIO_Schmit = GPIO_Schmit_Enable;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    // Set SYSCFG pin
    SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOC, EXTI_PinSource3);

    // Re-Float Rink-Mixed: what EXTI_Init() / NVIC_Init() write for these
    // settings, written directly (smaller, to make room for the bootloader).
    // EXTI line 3: interrupt (not event), falling edge only
    EXTI->IMR &= ~EXTI_Line3;
    EXTI->EMR &= ~EXTI_Line3;
    EXTI->IMR |= EXTI_Line3;
    EXTI->RTSR &= ~EXTI_Line3;
    EXTI->FTSR &= ~EXTI_Line3;
    EXTI->FTSR |= EXTI_Line3;

    // EXTI3 in the NVIC, priority 1
    hw_nvic_enable(EXTI3_IRQn, 1U);
}

bool button_driver_hw_is_pressed(void)
{
    return BUTTON_PRESSED;
}