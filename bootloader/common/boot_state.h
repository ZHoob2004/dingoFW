#ifndef DINGO_BOOT_STATE_H
#define DINGO_BOOT_STATE_H

/* Fourth word of the existing 16-byte OpenBLT reset handoff.
 * The application sets READY only after initialization completes.
 * The bootloader converts READY to WARM only on a non-power reset.
 * The application consumes WARM once; any other value requires cold startup.
 */
#define DINGO_BOOT_STATE_WORD       3u
#define DINGO_BOOT_STATE_READY      0xD190A11Du
#define DINGO_BOOT_STATE_WARM       0xD190B007u

#endif
