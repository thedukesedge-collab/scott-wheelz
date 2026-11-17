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
#include "vesc_serial_hw.h"
#include "hk32f030m.h"
#include "hw_regs.h"

void vesc_serial_hw_init(uint32_t baud)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOD, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);

    // PD1->TX
    GPIO_StructInit(&GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
    GPIO_Init(GPIOD, &GPIO_InitStructure);
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource1, GPIO_AF_1);

    // PD6->RX
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOD, &GPIO_InitStructure);
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource6, GPIO_AF_1);

    // Re-Float Rink-Mixed: the registers USART_Init() would write for 8N1,
    // RX+TX, no flow control, written directly - USART_Init() needs
    // RCC_GetClocksFreq() just to find PCLK (~320 bytes together). USART1
    // runs from PCLK = SystemCoreClock (SystemInit: USART1SW = PCLK, HCLK
    // and PCLK undivided). Same rounding as USART_Init().
    {
        uint32_t divider = SystemCoreClock / baud;

        if (SystemCoreClock % baud >= baud / 2U)
        {
            divider++;
        }
        USART1->CR1 &= ~USART_CR1_UE;
        USART1->CR2 &= ~USART_CR2_STOP;                                   // 1 stop bit
        USART1->CR1 = (USART1->CR1 & ~(USART_CR1_M | USART_CR1_PCE | USART_CR1_PS |
                                       USART_CR1_TE | USART_CR1_RE)) |
                      USART_CR1_TE | USART_CR1_RE;                        // 8 bits, no parity
        USART1->CR3 &= ~(USART_CR3_RTSE | USART_CR3_CTSE);                 // no flow control
        USART1->BRR = (uint16_t)divider;
    }
    USART_Cmd(USART1, ENABLE);

    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
    USART_ITConfig(USART1, USART_IT_IDLE, ENABLE);

    hw_nvic_enable(USART1_IRQn, 0U); // = NVIC_Init(), priority 0
}

/**
 * @brief Send data over the VESC serial hardware
 * @param[in] data pointer to the data to send
 * @param[in] len number of bytes to send
 *
 * This function sends the given data over the VESC serial hardware in a
 * blocking manner. It waits for the transmit data register to be empty
 * before sending the next byte.
 */
void vesc_serial_hw_send(uint8_t *data, uint16_t len)
{
    uint16_t i = 0;

    while (i < len)
    {
        while ((USART1->ISR & USART_ISR_TXE) == 0)
            ;
        USART1->TDR = data[i];
        i++;
    }
}
