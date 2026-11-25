# LINBoard v1 Dingo USB-CAN device

Build with `make BOARD=linboard_v1`. The STM32F302CCT6 uses the shared Dingo device core, CAN threads, configuration protocol and USB CDC transport. Initial programming uses SWD with the matching OpenBLT and application images. Build the bootloader with `make -C bootloader/stm32f302`. Subsequent updates use OpenBLT CAN or the ST ROM USB DFU entry command. LIN application functionality is not included.

Default base ID: **0x680**. Device type: **5** (4 is assigned to dingoPDM v8). Default CAN bitrate: **500 kbit/s**. The device uses the shared firmware parameter registry. Configuration occupies the final 2 KiB flash page at 0x0803F800. Write and burn changes before restarting to apply a new bitrate.

Select **LINBoard** in DingoConfig and the **USB** adapter for its COM port. The CAN controller runs independently of the host. The bridge supports standard CR-terminated CAN data frames; adapter initialization commands are ignored. Keep CAN filters disabled for general bus monitoring. Base+2 carries device state and heartbeat, and Base+3 carries optional network keep-awake requests.

Sleep Enabled and Network Keep-Awake both default to disabled. When automatic sleep is enabled, USB_ACTIVE and received traffic inhibit independent sleep. Distributed sleep uses the shared coordinated inactivity protocol. CAN RX (PB8) and USB data-line edges (PA11/PA12) provide wake sources; wake resets the MCU. USB attachment wake on this externally pulled-up F302 must be validated on hardware.

Hardware: PA11/PA12 USB, PB8/PB9 CAN, PC13 transceiver standby, PB2 status LED, PB0 receive activity LED, PB1 fatal error LED. USB descriptors include the product identity and full factory UID.

Validation: flash with configuration erased; verify USB enumeration, version and parameter read/write/burn, persisted bitrate and base ID, CAN traffic forwarding, sleep opt-in, CAN wake and USB wake. Record actual standby current.

## Bootloader

OpenBLT occupies 0x08000000-0x08003FFF; the application begins at 0x08004000 and must end before 0x0803F800. The final configuration page is excluded from the OpenBLT flash layout. The reset handoff uses the reserved final 16 bytes of SRAM at 0x20009FF0. CAN XCP IDs are base+12 (host to target) and base+13 (target to host), with the configured CAN bitrate passed across reset. Recovery uses base 0x680 at 500 kbit/s when no valid application is present. USB CDC and cyclic CAN status are provided by the application, not OpenBLT. To test LINBoard CAN firmware updates use a separate CAN adapter, since the LINBoard USB bridge is not running in OpenBLT. ROM DFU uses system memory 0x1FFFD800 (AN2606); an externally pulled-up USB connection may need to be replugged after entering DFU. Program the application at 0x08004000, not the bootloader address.

The exact STM32F302xC vector table is from STMicroelectronics cmsis-device-f3 (startup source blob b57b5113f231d874ff89b0ee6d367b770002df5d), with its accompanying license in bootloader/stm32f302/LICENSE.md. The HAL comes from the existing pinned OpenBLT dependency and the device header from pinned ChibiOS.
