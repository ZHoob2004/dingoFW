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
#include "can_frame.h"
#include "param_protocol.h"
#include <atomic>

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


// Dedicated network-management frame directly after each target's existing
// cyclic range: LINBoard +3, CANBoard +15, PDM v7 +55, PDM-Max +53, PT-DPDM4 +54, PDM v8 +39.
// Payload: 'D','N','G',protocol-version(1),base ID little-endian,
// request flags, rolling sequence.
namespace
{
constexpr uint32_t NETWORK_IDLE_MS = 30000;
constexpr uint32_t NETWORK_QUIET_MS = 2000;
constexpr uint32_t NETWORK_INPUT_HOLD_MS = 2000;
constexpr uint32_t NETWORK_BOOT_HOLD_MS = 2000;
constexpr uint32_t NETWORK_CYCLIC_ID_OFFSET =
    CYCLIC_TX_OFFSET + NUM_TX_MSGS;
constexpr uint8_t NETWORK_PROTOCOL_VERSION = 1;
constexpr uint8_t NETWORK_KEEP_AWAKE_FLAG = 0x01;

// Validate the ID against the sender's embedded base ID and a known
// device-specific offset, so peers need no configured device list.
constexpr uint16_t NETWORK_LINBOARD_OFFSET = 3;
constexpr uint16_t NETWORK_CANBOARD_OFFSET = 15;
constexpr uint16_t NETWORK_PDM_OFFSET = 55;
constexpr uint16_t NETWORK_PDM_MAX_OFFSET = 53;
constexpr uint16_t NETWORK_PT_PDM_OFFSET = 54;
constexpr uint16_t NETWORK_PDM_V8_OFFSET = 39;
static_assert(NETWORK_CYCLIC_ID_OFFSET == NETWORK_LINBOARD_OFFSET ||
              NETWORK_CYCLIC_ID_OFFSET == NETWORK_CANBOARD_OFFSET ||
              NETWORK_CYCLIC_ID_OFFSET == NETWORK_PDM_OFFSET ||
              NETWORK_CYCLIC_ID_OFFSET == NETWORK_PDM_MAX_OFFSET ||
              NETWORK_CYCLIC_ID_OFFSET == NETWORK_PT_PDM_OFFSET ||
              NETWORK_CYCLIC_ID_OFFSET == NETWORK_PDM_V8_OFFSET);

std::atomic<uint32_t> nLastNetworkRequest{0};
std::atomic<bool> bNetworkRuntimeEnabled{false};
std::atomic<bool> bLocalDemand{false};
uint32_t nLastInputChange;
uint32_t nNetworkStart;
uint16_t nPreviousInputMask;
bool bNetworkWasEnabled;

uint16_t EnabledDigitalInputMask()
{
    uint16_t mask = 0;
#if NUM_DIG_INPUTS > 0
    for (uint8_t i = 0; i < NUM_DIG_INPUTS; ++i)
        if (stConfig.stDigInput[i].bEnabled && GetDigInputVal(i))
            mask |= (1U << i);
#endif
    return mask;
}

// Only called in DeviceThread. CAN TX/RX consume atomically published state.
bool ComputeLocalKeepAwake()
{
    const uint32_t now = SYS_TIME;
    if (now - nNetworkStart < NETWORK_BOOT_HOLD_MS ||
        now - nLastInputChange < NETWORK_INPUT_HOLD_MS ||
        EnabledDigitalInputMask() != 0)
        return true;

    // Nodes configured to remain awake keep participating peers awake.
    if (!stConfig.stDevice.bSleepEnabled || IsParamOpInProgress())
        return true;
#if NUM_OUTPUTS > 0
    for (uint8_t i = 0; i < NUM_OUTPUTS; ++i)
        if (GetOutputState(i) != ProfetState::Off)
            return true;
#endif
#if NUM_DIG_OUTPUTS > 0
    for (uint8_t i = 0; i < NUM_DIG_OUTPUTS; ++i)
        if (GetDigOutputState(i))
            return true;
#endif
#if HAS_USB
    if (GetUsbConnected())
        return true;
#endif
    return false;
}
} // namespace

void NetworkSleepUpdate(bool additionalInputActivity)
{
    if (!bNetworkKeepAwakeEnabled)
    {
        bNetworkRuntimeEnabled.store(false, std::memory_order_relaxed);
        bLocalDemand.store(false, std::memory_order_relaxed);
        bNetworkWasEnabled = false;
        return;
    }

    const uint32_t now = SYS_TIME;
    if (!bNetworkWasEnabled)
    {
        nNetworkStart = now;
        nLastNetworkRequest.store(now, std::memory_order_relaxed);
        nLastInputChange = now;
        nPreviousInputMask = EnabledDigitalInputMask();
        bNetworkWasEnabled = true;
    }

    const uint16_t inputs = EnabledDigitalInputMask();
    if (inputs != nPreviousInputMask || additionalInputActivity)
    {
        nPreviousInputMask = inputs;
        nLastInputChange = now;
    }

    const bool demand = ComputeLocalKeepAwake();
    bLocalDemand.store(demand, std::memory_order_relaxed);
    bNetworkRuntimeEnabled.store(true, std::memory_order_relaxed);
    // Remote requests refresh the inactivity timer without asserting local demand.
    if (demand)
        nLastNetworkRequest.store(now, std::memory_order_relaxed);
}

void NetworkProcessCanFrame(const CANRxFrame& frame)
{
    if (!bNetworkRuntimeEnabled.load(std::memory_order_relaxed))
        return;
    if (CanIsExt(frame) || CanIsRemote(frame) ||
        frame.DLC != 8 ||
        frame.data8[0] != 'D' || frame.data8[1] != 'N' ||
        frame.data8[2] != 'G' || frame.data8[3] != NETWORK_PROTOCOL_VERSION ||
        (frame.data8[6] & ~NETWORK_KEEP_AWAKE_FLAG) != 0)
        return;

    const uint16_t base = static_cast<uint16_t>(frame.data8[4]) |
                          (static_cast<uint16_t>(frame.data8[5]) << 8);
    const uint32_t id = CanId(frame);
    if (base > 0x7FF || id < base)
        return;
    const uint16_t offset = id - base;
    if (offset != NETWORK_LINBOARD_OFFSET &&
        offset != NETWORK_CANBOARD_OFFSET &&
        offset != NETWORK_PDM_OFFSET &&
        offset != NETWORK_PDM_MAX_OFFSET &&
        offset != NETWORK_PT_PDM_OFFSET &&
        offset != NETWORK_PDM_V8_OFFSET)
        return;

    if ((frame.data8[6] & NETWORK_KEEP_AWAKE_FLAG) != 0)
        nLastNetworkRequest.store(SYS_TIME, std::memory_order_relaxed);
}

bool NetworkBuildKeepAwakeFrame(CANTxFrame& frame)
{
    // Called only from the cyclic CAN transmit thread.
    if (!bNetworkRuntimeEnabled.load(std::memory_order_relaxed) ||
        !bLocalDemand.load(std::memory_order_relaxed))
        return false;

    const uint32_t id = stConfig.stDevice.nBaseId + NETWORK_CYCLIC_ID_OFFSET;
    if (id > 0x7FF)
        return false;

    static uint8_t sequence = 0;
    CanSetId(frame, id);
    frame.DLC = 8;
    CanClearTxFlags(frame);
    frame.data8[0] = 'D';
    frame.data8[1] = 'N';
    frame.data8[2] = 'G';
    frame.data8[3] = NETWORK_PROTOCOL_VERSION;
    frame.data8[4] = static_cast<uint8_t>(stConfig.stDevice.nBaseId);
    frame.data8[5] = static_cast<uint8_t>(stConfig.stDevice.nBaseId >> 8);
    frame.data8[6] = NETWORK_KEEP_AWAKE_FLAG;
    frame.data8[7] = sequence++;
    return true;
}

bool NetworkFeatureEnabled()
{
    return bNetworkRuntimeEnabled.load(std::memory_order_relaxed);
}

bool NetworkCyclicTxAllowed()
{
    if (!bNetworkRuntimeEnabled.load(std::memory_order_relaxed) ||
        bLocalDemand.load(std::memory_order_relaxed))
        return true;
    const uint32_t last = nLastNetworkRequest.load(std::memory_order_relaxed);
    return (SYS_TIME - last) < (NETWORK_IDLE_MS - NETWORK_QUIET_MS);
}

bool NetworkSleepCanEnter()
{
    if (!bNetworkRuntimeEnabled.load(std::memory_order_relaxed) ||
        bLocalDemand.load(std::memory_order_relaxed))
        return false;
    // Read timestamps before sampling now, avoiding unsigned underflow
    // when an interrupt/thread publishes a newer timestamp concurrently.
    const uint32_t request = nLastNetworkRequest.load(std::memory_order_relaxed);
    const uint32_t rx = GetLastCanRxTime();
    const uint32_t tx = GetLastCanTxTime();
    const uint32_t now = SYS_TIME;
    if (now - request < NETWORK_IDLE_MS)
        return false;
    // After cyclic traffic pauses, require 2s without accepted RX or TX
    // mailbox submissions before STOP.
    if (now - rx < NETWORK_QUIET_MS || now - tx < NETWORK_QUIET_MS)
        return false;
    return true;
}

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

    // Peer keep-awake requests replace the raw CAN RX inactivity timeout.
    // Local inhibitors and explicit sleep requests retain their meaning.
    if (bNetworkKeepAwakeEnabled)
    {
        bEnterSleep = stConfig.stDevice.bSleepEnabled && nNumActiveSignals == 0 &&
            #if HAS_USB
            !GetUsbConnected() &&
            #endif
            !IsParamOpInProgress() && NetworkSleepCanEnter();
    }

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

    #if NUM_DIG_INPUTS > 0
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

    #endif

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
    #if defined(STM32F303x8) || defined(STM32F302xC)
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