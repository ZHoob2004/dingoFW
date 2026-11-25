#pragma once

#include "hal.h"
#include "enums.h"

// Use the regular Dingo device/configuration and USB-CAN paths.
#define LINBOARD_V1 1
#define DEVICE_TYPE 5
#define PROCESS_STACK 0x0400

#define HAS_SE_LEDS TRUE
#define HAS_USB TRUE
#define HAS_I2C1 FALSE
#define HAS_I2C2 FALSE
#define HAS_EXT_TEMP_SENSOR FALSE
#define HAS_EXT_MEMORY FALSE
#define HAS_BATT_VOLT_SENSE FALSE
#define CAN_SLEEP TRUE
#define HAS_WIPERS FALSE
#define HAS_STARTER_DISABLE FALSE
#define HAS_NEOPIXELS FALSE

#define NUM_OUTPUTS 0
#define NUM_DIG_INPUTS 0
#define NUM_DIG_OUTPUTS 0
#define NUM_ANALOG_INPUTS 0
#define NUM_VIRT_INPUTS 0
#define NUM_CAN_INPUTS 0
#define NUM_CAN_OUTPUTS 0
#define NUM_CAN_BUSES 1
#define NUM_TIMERS 0
#define NUM_MATH 0
#define HAS_LUA FALSE
#define NUM_LUA_OUTPUTS 0
#define NUM_FLASHERS 0
#define NUM_COUNTERS 0
#define NUM_CONDITIONS 0
#define NUM_KEYPADS 0
#define NUM_WIPER_INTER_DELAYS 0
#define NUM_WIPER_SPEED_MAP 0
#define KEYPAD_MAX_BUTTONS 0
#define KEYPAD_MAX_ANALOG_INPUTS 0
#define KEYPAD_MAX_DIALS 0

#define VAR_MAP_SIZE 3
#define VAR_MAP_SYS_VARS 3
#define VAR_MAP_WIPER_VARS 0

// F302CCT6: 256 KiB flash, 2 KiB pages. Reserve the final page.
#define CONFIG_SECTOR 127U
#define CONFIG_NUM_SECTORS 1U
#define CONFIG_FLASH_OFFSET (CONFIG_SECTOR * 2048U)
#define CONFIG_FLASH getBaseFlash(&EFLD1)

#define MAILBOX_SIZE 128
#define DEVICE_THREAD_STACK 1024
#define CAN_THREAD_STACK 256
#define NUM_TX_MSGS 1
#define DEFAULT_BASE_ID 0x680
#define SLEEP_TIMEOUT 30000 // ms
#define SYS_TIME ((uint32_t)(chVTGetTimeStamp() / (CH_CFG_ST_FREQUENCY / 1000)))

static const float ALWAYS_FALSE = 0.0f;
static const float ALWAYS_TRUE = 1.0f;

const CANConfig& GetCanConfig(CanBitrate bitrate);
msg_t InitAdc();
