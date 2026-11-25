#pragma once

// STM32F302 EXTI pending register for lines 0-31.
#define EXTI_PENDING EXTI->PR

// Reserved top of SRAM, shared with the F302 OpenBLT port.
#define BL_HANDOFF_ADDR 0x20009FF0u

void EnterStopMode();
void RequestBootloader();
