#pragma once

#include <cstddef>
#include <cstdint>
#include "crc.h"

// Store network settings independently to preserve the device config layout.
// The record occupies two 8-byte STM32G4 flash programming units.
constexpr uint32_t NETWORK_CONFIG_MAGIC = 0x4E4B4131U;  // "NKA1"
struct NetworkConfigRecord
{
    uint32_t magic;
    uint32_t enabled;
    uint32_t crc;
    uint32_t reserved;
};
static_assert(sizeof(NetworkConfigRecord) == 16);

inline NetworkConfigRecord MakeNetworkConfigRecord(bool enabled)
{
    NetworkConfigRecord record{NETWORK_CONFIG_MAGIC, enabled ? 1U : 0U, 0U, 0U};
    record.crc = CalculateCRC32(&record, offsetof(NetworkConfigRecord, crc));
    return record;
}

inline bool IsValidNetworkConfigRecord(const NetworkConfigRecord& record)
{
    return record.magic == NETWORK_CONFIG_MAGIC && record.enabled <= 1U &&
           record.crc == CalculateCRC32(&record, offsetof(NetworkConfigRecord, crc));
}
