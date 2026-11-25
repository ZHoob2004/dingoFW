DEMO      := ../openblt/Target/Demo/ARMCM4_STM32F3_Nucleo_F303K8_CubeIDE/Boot
PROJ      := openblt_stm32f302
FAMILY    := F3
DEVICE    := STM32F302xC
HAL_DIR   := $(DEMO)/Drivers/STM32F3xx_HAL_Driver
HAL_CONF  := $(DEMO)/Core/Inc
CMSIS_DIR := $(DEMO)/Drivers/CMSIS
SYSTEM_C  := $(DEMO)/Core/Src/system_stm32f3xx.c
STARTUP_S := startup_stm32f302xc.s

# The F3 demo provides the HAL and CMSIS core; ChibiOS provides the F302
# device header required for this target.
EXTRA_INC := ../../ChibiOS/os/common/ext/ST/STM32F3xx

include ../common/rules.mk
