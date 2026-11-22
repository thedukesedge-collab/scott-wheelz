/*
 * GCC startup for the HK32F030M (Cortex-M0), equivalent to
 * KEIL_Startup_hk32f030m.s: same vector table order, Reset_Handler calls
 * SystemInit() then main() after setting up .data/.bss.
 *
 * Part of the Refloat Glow GCC build of ALCM (GPL-3.0-or-later).
 */
#include <stdint.h>

extern uint32_t _sidata, _sdata, _edata, _sbss, _ebss, _estack;

extern void SystemInit(void);
extern int main(void);

void Reset_Handler(void);
void Default_Handler(void);

#define WEAK_ALIAS __attribute__((weak, alias("Default_Handler")))
void NMI_Handler(void) WEAK_ALIAS;
void HardFault_Handler(void) WEAK_ALIAS;
void SVC_Handler(void) WEAK_ALIAS;
void PendSV_Handler(void) WEAK_ALIAS;
void SysTick_Handler(void) WEAK_ALIAS;
void WWDG_IRQHandler(void) WEAK_ALIAS;
void EXTI11_IRQHandler(void) WEAK_ALIAS;
void FLASH_IRQHandler(void) WEAK_ALIAS;
void RCC_IRQHandler(void) WEAK_ALIAS;
void EXTI0_IRQHandler(void) WEAK_ALIAS;
void EXTI1_IRQHandler(void) WEAK_ALIAS;
void EXTI2_IRQHandler(void) WEAK_ALIAS;
void EXTI3_IRQHandler(void) WEAK_ALIAS;
void EXTI4_IRQHandler(void) WEAK_ALIAS;
void EXTI5_IRQHandler(void) WEAK_ALIAS;
void TIM1_BRK_IRQHandler(void) WEAK_ALIAS;
void ADC1_IRQHandler(void) WEAK_ALIAS;
void TIM1_UP_TRG_COM_IRQHandler(void) WEAK_ALIAS;
void TIM1_CC_IRQHandler(void) WEAK_ALIAS;
void TIM2_IRQHandler(void) WEAK_ALIAS;
void TIM6_IRQHandler(void) WEAK_ALIAS;
void EXTI6_IRQHandler(void) WEAK_ALIAS;
void EXTI7_IRQHandler(void) WEAK_ALIAS;
void I2C1_IRQHandler(void) WEAK_ALIAS;
void SPI1_IRQHandler(void) WEAK_ALIAS;
void USART1_IRQHandler(void) WEAK_ALIAS;

__attribute__((section(".isr_vector"), used)) void (*const g_pfnVectors[])(void) = {
    (void (*)(void))(&_estack), // Top of stack
    Reset_Handler,
    NMI_Handler,
    HardFault_Handler,
    0, 0, 0, 0, 0, 0, 0,        // Reserved
    SVC_Handler,
    0, 0,                       // Reserved
    PendSV_Handler,
    SysTick_Handler,
    WWDG_IRQHandler,            // Window Watchdog
    0,                          // Reserved
    EXTI11_IRQHandler,          // EXTI Line 11 (AWU_WKP)
    FLASH_IRQHandler,
    RCC_IRQHandler,
    EXTI0_IRQHandler,
    EXTI1_IRQHandler,
    EXTI2_IRQHandler,
    EXTI3_IRQHandler,
    EXTI4_IRQHandler,
    EXTI5_IRQHandler,
    TIM1_BRK_IRQHandler,
    ADC1_IRQHandler,            // ADC1 (combined with EXTI line 8)
    TIM1_UP_TRG_COM_IRQHandler,
    TIM1_CC_IRQHandler,
    TIM2_IRQHandler,
    0,                          // Reserved
    TIM6_IRQHandler,
    0, 0, 0,                    // Reserved
    EXTI6_IRQHandler,
    EXTI7_IRQHandler,
    I2C1_IRQHandler,            // I2C1 (combined with EXTI line 10)
    0,                          // Reserved
    SPI1_IRQHandler,
    0,                          // Reserved
    USART1_IRQHandler,          // USART1 (combined with EXTI line 9)
    0, 0, 0, 0,                 // Reserved
};

void Reset_Handler(void)
{
    uint32_t *src = &_sidata;
    uint32_t *dst = &_sdata;

    while (dst < &_edata)
    {
        *dst++ = *src++;
    }
    for (dst = &_sbss; dst < &_ebss;)
    {
        *dst++ = 0U;
    }

    SystemInit();
    (void)main();
    for (;;)
    {
    }
}

void Default_Handler(void)
{
    for (;;)
    {
    }
}
