MEMORY
{
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 16K /* App starts at 0x08004000 */
  RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 16K /* Leave the handoff block untouched */
}

INCLUDE sections.ld
