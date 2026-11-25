// LINBoard v1, STM32F302CCT6: OpenBLT CAN and ST ROM USB DFU.
#ifndef BLT_CONF_H
#define BLT_CONF_H

#define DINGO_DEVICE_H        "stm32f3xx.h"
#define DINGO_DEFAULT_BASE_ID (0x680u)
#define DINGO_HANDOFF_ADDR    (0x20009FF0u) // Reserved top 16 bytes of SRAM
#define DINGO_SYSTEM_MEMORY   (0x1FFFD800u) // AN2606, STM32F302xB(C)
#define DINGO_RAM_START       (0x20000000u)
#define DINGO_RAM_END         (0x2000A000u)
#define DINGO_APP_END         (0x0803F800u) // Final 2 KiB page is configuration

#define BOOT_NVM_SIZE_KB      (256)

#include "dingo_blt.h"

#endif
