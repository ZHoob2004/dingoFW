#pragma once

#include "ch.h"
#include "hal.h"
#include "config.h"
#include "enums.h"

// Sleep management functions extracted from pdm.cpp

bool CheckEnterSleep();
void EnterSleep();
void EnableLineEventWithPull(ioline_t line, InputPull pull);

// Optional network sleep coordination.
void NetworkSleepUpdate(bool additionalInputActivity = false);
bool NetworkSleepCanEnter();
bool NetworkCyclicTxAllowed();
bool NetworkFeatureEnabled();
void NetworkProcessCanFrame(const CANRxFrame& frame);
bool NetworkBuildKeepAwakeFrame(CANTxFrame& frame);
