#include "sleep.h"
#include "device.h"
#include "port.h"
#include "can.h"
#if HAS_USB
#include "usb.h"
#endif
#include "hw_devices.h"
#include "status.h"
#include "mcu_utils.h"
#include "device_config.h"

#if CAN_SLEEP
// Active logical signals inhibit automatic sleep.
static uint8_t nNumActiveSignals;
static uint8_t nLastNumActiveSignals;
static uint32_t nAllOutputsOffTime;
#if HAS_USB
static bool bLastUsbConnected;
static uint32_t nUsbDisconnectedTime;
#endif

// External variables from pdm.cpp that we need access to
extern DeviceConfig stConfig;
bool bSleepRequest;


bool CheckEnterSleep()
{
    bool bEnterSleep = false;

    // Count active outputs and CANBoard logical digital inputs.
    nNumActiveSignals = 0;
    #if NUM_OUTPUTS > 0
    for (int i = 0; i < NUM_OUTPUTS; i++)
    {
        if (GetOutputState(i) != ProfetState::Off)
            nNumActiveSignals++;
    }
    #endif
    #if NUM_DIG_OUTPUTS > 0
    for (uint8_t i = 0; i < NUM_DIG_OUTPUTS; i++)
    {
        if (GetDigOutputState(i))
            nNumActiveSignals++;
    }
    #endif
    #if NUM_DIG_INPUTS > 0 && NUM_OUTPUTS == 0
    for (uint8_t i = 0; i < NUM_DIG_INPUTS; i++)
    {
        // Include inversion, debounce, latching and configured PWM detection.
        if (stConfig.stDigInput[i].bEnabled && GetDigInputVal(i))
            nNumActiveSignals++;
    }
    #endif

    // Start the full inactivity timeout when the last active signal clears.
    if ((nNumActiveSignals == 0) && (nLastNumActiveSignals > 0))
    {
        nAllOutputsOffTime = SYS_TIME;
    }
    nLastNumActiveSignals = nNumActiveSignals;

    #if HAS_USB
    //USB disconnected, save time - wait SLEEP_TIMEOUT before sleep
    if (!GetUsbConnected() && bLastUsbConnected)
        nUsbDisconnectedTime = SYS_TIME;

    bLastUsbConnected = GetUsbConnected();
    #endif

    // CAN Rx thread can stamp a time slightly after SYS_TIME was read here,
    // so treat a negative (signed) difference as 0. Wrap-safe.
    int32_t nCanRxDiff = static_cast<int32_t>(SYS_TIME - GetLastCanRxTime());
    uint32_t nCanRxIdleTime = (nCanRxDiff > 0) ? nCanRxDiff : 0;

    // No active signals, no CAN msgs received and no USB connected
    // Go to sleep after timeout
    bEnterSleep = stConfig.stDevice.bSleepEnabled &&
                  (nNumActiveSignals == 0) &&
                  (nLastNumActiveSignals == 0) &&
                  #if HAS_USB
                  !GetUsbConnected() &&
                  ((SYS_TIME - nUsbDisconnectedTime) > SLEEP_TIMEOUT) &&
                  #endif
                  ((SYS_TIME - nAllOutputsOffTime) > SLEEP_TIMEOUT) &&
                  (nCanRxIdleTime > SLEEP_TIMEOUT);

    return bEnterSleep || bSleepRequest || *pVarMap[stConfig.stDevice.nForceSleepInput];
}

void EnableLineEventWithPull(ioline_t line, InputPull pull) 
{
    // Bias the GPIO through its pin mode, independently of EXTI edge selection.
    switch(pull) {
        case InputPull::Up:
            palSetLineMode(line, PAL_MODE_INPUT_PULLUP);
            break;
        case InputPull::Down:
            palSetLineMode(line, PAL_MODE_INPUT_PULLDOWN);
            break;
        default:
            palSetLineMode(line, PAL_MODE_INPUT);
            break;
    }
    palEnableLineEvent(line, PAL_EVENT_MODE_BOTH_EDGES);
}

void EnterSleep()
{
    // Clear CAN sleep request so we don't immediately re-enter sleep after waking up
    bSleepRequest = false;

    // Forced sleep can arrive with outputs on - shut them off, no current protection while stopped
    #if NUM_OUTPUTS > 0
    for (uint8_t i = 0; i < NUM_OUTPUTS; i++)
        pf[i].ForceOff();
    #endif
    #if NUM_DIG_OUTPUTS > 0
    for (uint8_t i = 0; i < NUM_DIG_OUTPUTS; i++)
        digOut[i].ForceOff();
    #endif

    // Stop transmitting before the transceiver goes to standby. A frame still
    // going out while another device on the bus falls asleep wakes it up again.
    // Frames already in the CAN mailboxes finish during the wait below.
    SetCanTxQuiet(true);
    chThdSleepMilliseconds(100);

    // Stop the CAN controller before its pins change. Left running, it sees the
    // RX pin taken away from it and answers with an error frame, and the error
    // flag the other nodes send back arrives just after the transceiver went to
    // standby - which wakes this device straight back up whenever anything else
    // is on the bus.
    StopCan();
    palSetLineMode(LINE_CAN_TX, PAL_MODE_OUTPUT_PUSHPULL);
    palSetLine(LINE_CAN_TX); // Recessive

    palSetLine(LINE_CAN_STANDBY); // CAN disabled

    // Let the transceiver settle in standby before any wake source is armed
    chThdSleepMilliseconds(20);

    // A PWM input's edge event would wake it, and the wake event below needs the same EXTI line
    for(uint8_t i = 0; i < NUM_DIG_INPUTS; i++)
        digIn[i].StopPwm();

    // Set wakeup sources

    // Digital inputs change detection, with configured pullup or pulldown
    // Can be disabled in config
    for(uint8_t i = 0; i < NUM_DIG_INPUTS; i++)
    {
        #if NUM_DIG_OUTPUTS > 0
        // Unconfigured CANBoard inputs must not wake the MCU or draw pull current.
        if (!stConfig.stDigInput[i].bEnabled)
        {
            palSetLineMode(digIn[i].GetLine(), PAL_MODE_INPUT_ANALOG);
            continue;
        }
        #endif
        if (!stConfig.stDevice.bDisableDigInWake)
            EnableLineEventWithPull(digIn[i].GetLine(), stConfig.stDigInput[i].ePull);
    }

    // CAN receive detection - can be disabled in config
    if(!stConfig.stDevice.bDisableCanWake)
    {
        palSetLineMode(LINE_CAN_RX, PAL_MODE_INPUT);
        palEnableLineEvent(LINE_CAN_RX, PAL_EVENT_MODE_BOTH_EDGES | PAL_STM32_PUPDR_FLOATING);
    }

    #if HAS_USB
    // USB detection - always active
    palSetLineMode(LINE_USB_DP, PAL_MODE_INPUT);
    palEnableLineEvent(LINE_USB_DP, PAL_EVENT_MODE_BOTH_EDGES | PAL_STM32_PUPDR_FLOATING);
    palSetLineMode(LINE_USB_DM, PAL_MODE_INPUT);
    palEnableLineEvent(LINE_USB_DM, PAL_EVENT_MODE_BOTH_EDGES | PAL_STM32_PUPDR_FLOATING);
    #endif

    // Arming a line can latch an edge from the pin changing mode. Clear those so
    // only a real wake source ends the stop. One arriving from here on is still
    // pending at the WFI and wakes the MCU as it should.
    EXTI_PENDING = 0x0000FFFF;
    NVIC_ClearPendingIRQ(EXTI0_IRQn);
    NVIC_ClearPendingIRQ(EXTI1_IRQn);
    #if defined(STM32F303x8)
    NVIC_ClearPendingIRQ(EXTI2_TSC_IRQn);
    #else
    NVIC_ClearPendingIRQ(EXTI2_IRQn);
    #endif
    NVIC_ClearPendingIRQ(EXTI3_IRQn);
    NVIC_ClearPendingIRQ(EXTI4_IRQn);
    NVIC_ClearPendingIRQ(EXTI9_5_IRQn);
    NVIC_ClearPendingIRQ(EXTI15_10_IRQn);

    EnterStopMode();
}

#endif