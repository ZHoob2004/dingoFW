#include "msg.h"
#include "can_frame.h"
#include "config.h"
#include "device_config.h"
#include "status.h"
#include "can.h"

CANTxMsg TxMsg0()
{
    static uint8_t heartbeat;
    CANTxMsg msg = {};
    CanSetId(msg.frame, stConfig.stDevice.nBaseId + CYCLIC_TX_OFFSET);
    msg.frame.DLC = 8;
    msg.frame.data8[0] = static_cast<uint8_t>(GetDeviceState());
    msg.frame.data8[7] = heartbeat++;
    msg.bSend = true;

    // RX activity indicator, independent of USB and host session.
    const uint32_t lastRx = GetLastCanRxTime();
    if (GetDeviceState() == DeviceState::Run && lastRx != 0 &&
        static_cast<uint32_t>(SYS_TIME - lastRx) < 150U)
        palSetLine(LINE_CAN_LED);
    else
        palClearLine(LINE_CAN_LED);
    return msg;
}
