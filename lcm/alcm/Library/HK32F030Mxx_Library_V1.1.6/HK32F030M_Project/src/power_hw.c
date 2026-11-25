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
#include "power_hw.h"
#include "hk32f030m.h"
#include "hw_regs.h"

void power_hw_init(void)
{
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOA, ENABLE);

    // PWR_EN (PA2)
    hw_gpio_pin(GPIOA, 2U, HW_GPIO_OUT);
    GPIO_PinLockConfig(GPIOA, GPIO_Pin_2);

    // Charge (PC5)
    hw_gpio_pin(GPIOC, 5U, HW_GPIO_OUT);
    GPIO_PinLockConfig(GPIOC, GPIO_Pin_5);
}

// Refloat Glow: see hw_regs.h (lives here: a file both builds compile)
void hw_gpio_pin(GPIO_TypeDef *gpio, uint32_t pin, uint32_t cfg)
{
    uint32_t shift = pin * 2U;
    uint32_t mask = 3UL << shift;
    uint32_t mode = cfg & 3U;

    if ((mode == GPIO_Mode_OUT) || (mode == GPIO_Mode_AF))
    {
        gpio->OSPEEDR = (gpio->OSPEEDR & ~mask) | (((cfg >> 3) & 3U) << shift);
        gpio->OTYPER = (gpio->OTYPER & ~(1UL << pin)) | (((cfg >> 2) & 1U) << pin);
    }
    gpio->MODER = (gpio->MODER & ~mask) | (mode << shift);
    gpio->PUPDR = (gpio->PUPDR & ~mask) | (((cfg >> 5) & 3U) << shift);
    if ((cfg & 0x80U) == 0U)
    {
        gpio->IOSR |= 1UL << pin; // Schmitt trigger off (vendor default)
    }
    else
    {
        gpio->IOSR &= ~(1UL << pin);
    }
}

void power_hw_set_power(power_hw_t power_hw)
{
    switch (power_hw)
    {
    case POWER_HW_ON:
        GPIOA->BSRR = GPIO_Pin_2;
        break;

    case POWER_HW_OFF:
        GPIOA->BRR = GPIO_Pin_2;
        break;

    default:
        break;
    }
}

void power_hw_set_charge(power_hw_t power_hw)
{
    switch (power_hw)
    {
    case POWER_HW_ON:
        GPIOC->BSRR = GPIO_Pin_5;
        break;

    case POWER_HW_OFF:
        GPIOC->BRR = GPIO_Pin_5;
        break;

    default:
        break;
    }
}