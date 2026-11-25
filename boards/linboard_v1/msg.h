#pragma once
#include "port.h"
#include "mailbox.h"

struct CANTxMsg
{
    CANTxFrame frame;
    bool bSend;
};

CANTxMsg TxMsg0();
[[maybe_unused]] static CANTxMsg (*TxMsgs[NUM_TX_MSGS])() = {TxMsg0};
