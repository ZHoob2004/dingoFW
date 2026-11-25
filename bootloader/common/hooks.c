// OpenBLT hooks shared by every dingo port, settings in dingo_blt.h
#include "boot.h"
#include "boot_state.h"
#include DINGO_DEVICE_H

static blt_bool bStay;
static blt_int32u nBaseId = DINGO_DEFAULT_BASE_ID;
static blt_int32u nBaudrate = 500000u;

#ifdef DINGO_SYSTEM_MEMORY
// ST ROM bootloader, USB DFU. Was the app's enter_bootloader.S, now the bootloader owns the reset vector
static void EnterDfu(void)
{
  RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
  SYSCFG->MEMRMP = 1u; // System memory at 0
  __set_MSP(((blt_int32u *)DINGO_SYSTEM_MEMORY)[0]);
  ((void (*)(void))((blt_int32u *)DINGO_SYSTEM_MEMORY)[1])();
}
#endif

// Capture reset cause before HAL_MspDeInit() clears RCC reset flags.
static void DingoStartupStateInit(volatile blt_int32u *pHandoff)
{
  const blt_int32u nResetFlags = RCC->CSR;
  blt_int32u nPowerResetMask = 0;
#ifdef RCC_CSR_PORRSTF
  nPowerResetMask |= RCC_CSR_PORRSTF;
#endif
#ifdef RCC_CSR_BORRSTF
  nPowerResetMask |= RCC_CSR_BORRSTF;
#endif
#ifdef RCC_CSR_V18PWRRSTF
  nPowerResetMask |= RCC_CSR_V18PWRRSTF;
#endif

  const blt_int32u nWarmResetMask = RCC_CSR_SFTRSTF | RCC_CSR_PINRSTF |
                                   RCC_CSR_IWDGRSTF | RCC_CSR_WWDGRSTF;
  // A reset during cold initialization, an unknown reset cause or a power
  // reset must retain the full delay, even if SRAM retained an old marker.
  const blt_bool bWarm = (pHandoff[DINGO_BOOT_STATE_WORD] == DINGO_BOOT_STATE_READY) &&
                         (nPowerResetMask != 0u) &&
                         ((nResetFlags & nPowerResetMask) == 0u) &&
                         ((nResetFlags & nWarmResetMask) != 0u) &&
                         ((nResetFlags & (RCC_CSR_LPWRRSTF
#ifdef RCC_CSR_OBLRSTF
                                          | RCC_CSR_OBLRSTF
#endif
                                          )) == 0u);
  pHandoff[DINGO_BOOT_STATE_WORD] = bWarm ? DINGO_BOOT_STATE_WARM : 0u;

  RCC->CSR |= RCC_CSR_RMVF;
  __DSB();
}

// Called first thing in main(), before the CAN driver reads the ids and bitrate
void DingoHandoffInit(void)
{
  // CanBitrate enum order, core/enums.h
  static const blt_int32u aBaud[] = { 1000000u, 500000u, 250000u, 125000u, 100000u };
  volatile blt_int32u *pHandoff = (volatile blt_int32u *)DINGO_HANDOFF_ADDR;
  DingoStartupStateInit(pHandoff);

#ifdef DINGO_SYSTEM_MEMORY
  if (pHandoff[0] == DINGO_BL_DFU_MAGIC)
  {
    pHandoff[0] = 0;
    EnterDfu();
  }
#endif

  if (pHandoff[0] != DINGO_BL_CAN_MAGIC)
    return;

  pHandoff[0] = 0; // Consumed, the reset after programming starts the new app
  bStay = BLT_TRUE;

  if (pHandoff[1] <= 0x7FFu - DINGO_XCP_TX_OFFSET)
    nBaseId = pHandoff[1];
  if (pHandoff[2] < sizeof(aBaud) / sizeof(aBaud[0]))
    nBaudrate = aBaud[pHandoff[2]];
}

blt_int32u DingoCanBaudrate(void) { return nBaudrate; }
blt_int32u DingoXcpTxId(void) { return nBaseId + DINGO_XCP_TX_OFFSET; }
blt_int32u DingoXcpRxId(void) { return nBaseId + DINGO_XCP_RX_OFFSET; }

void BackDoorInitHook(void)
{
}

blt_bool BackDoorEntryHook(void)
{
  return bStay;
}

// The host erases the app first, so the vectors read 0xFF until FlashDone() writes the first block back
blt_bool NvmVerifyChecksumHook(void)
{
  blt_int32u *pVectors = (blt_int32u *)NvmGetUserProgBaseAddress();
  blt_int32u nSp = pVectors[0];
  blt_int32u nReset = pVectors[1];

  return (nSp > DINGO_RAM_START) && (nSp <= DINGO_RAM_END) &&
         (nReset & 1u) && // Thumb
         (nReset > (blt_int32u)pVectors) && (nReset < DINGO_APP_END);
}

blt_bool NvmWriteChecksumHook(void)
{
  return BLT_TRUE;
}

// Linked with -nostartfiles (newlib's crt0 drags in exit() and stdio), the ST startup's __libc_init_array still wants it
void _init(void)
{
}
