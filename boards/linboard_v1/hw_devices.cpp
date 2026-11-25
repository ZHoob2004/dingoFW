#include "hw_devices.h"

Led statusLed(LINE_LED_STATUS);
// Use the LIN activity LED as a fatal-error indicator for the USB-CAN firmware.
Led errorLed(LINE_LIN_LED);
