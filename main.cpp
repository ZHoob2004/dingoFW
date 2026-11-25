#include "ch.h"
#include "hal.h"
#include "device.h"
#include "mcu_utils.h"
#include "bootloader/common/boot_state.h"

/*
 * Application entry point.
 */
int main(void) {
  
  volatile uint32_t *pBootState = reinterpret_cast<volatile uint32_t *>(BL_HANDOFF_ADDR) + DINGO_BOOT_STATE_WORD;
  const bool bWarmBoot = *pBootState == DINGO_BOOT_STATE_WARM;
  *pBootState = 0;
  __DSB();

  halInit();
  chSysInit();

  // Allow FRAM and other peripherals to settle after power-up. OpenBLT
  // confirms a warm reset only after the previous initialization completed.
  if (!bWarmBoot)
    chThdSleepMilliseconds(500);

  InitDevice();

  *pBootState = DINGO_BOOT_STATE_READY;
  __DSB();
  
  while (true) {
    chThdSleepMilliseconds(500);
  }
}
