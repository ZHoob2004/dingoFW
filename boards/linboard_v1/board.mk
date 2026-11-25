MCU = cortex-m4
MCUDIR = boards/stm32f302
USE_FPU = hard
# ChibiOS driver casts require alias-safe compilation on optimized builds.
USE_OPT := $(USE_OPT) -fno-strict-aliasing
BOARDSRC = ./boards/linboard_v1/board.c
BOARDINC = ./boards/linboard_v1
ALLCSRC += $(BOARDSRC)
ALLINC += $(BOARDINC)
include $(CHIBIOS)/os/common/startup/ARMCMx/compilers/GCC/mk/startup_stm32f3xx.mk
include $(CHIBIOS)/os/hal/ports/STM32/STM32F3xx/platform.mk
include $(CHIBIOS)/os/common/ports/ARMv7-M/compilers/GCC/mk/port.mk

CPPSRC_BOARD += comms/usb.cpp core/sleep.cpp
