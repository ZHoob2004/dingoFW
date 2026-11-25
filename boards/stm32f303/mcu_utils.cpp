#include "mcu_utils.h"
#include "hal.h"

// Leave only the EXTI wake interrupts enabled so a pending timer or CAN IRQ
// cannot cause WFI to return before the MCU enters STOP.
static void QuietNvic()
{
    SysTick->CTRL = 0;
    for (uint32_t i = 0; i < 8; i++)
    {
        NVIC->ICER[i] = 0xFFFFFFFFu;
        NVIC->ICPR[i] = 0xFFFFFFFFu;
    }
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk;
    static const IRQn_Type wake[] = {EXTI0_IRQn, EXTI1_IRQn, EXTI2_TSC_IRQn, EXTI3_IRQn, EXTI4_IRQn, EXTI9_5_IRQn, EXTI15_10_IRQn};
    for (IRQn_Type n : wake)
        NVIC_EnableIRQ(n);
}

void EnterStopMode()
{
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    (void)RCC->APB1ENR;
    PWR->CR &= ~PWR_CR_PDDS;
    PWR->CR |= PWR_CR_CWUF | PWR_CR_CSBF | PWR_CR_LPDS;
    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;

    __disable_irq();
    QuietNvic();
    __DSB();
    __WFI();

    // Reset to restore clocks and peripherals after wake.
    NVIC_SystemReset();
}
