// First 16 KiB is OpenBLT; the final 2 KiB page is application configuration.
static const tFlashSector flashLayout[] =
{
  { 0x08004000, 0x4000 },
  { 0x08008000, 0x8000 },
  { 0x08010000, 0x10000 },
  { 0x08020000, 0x10000 },
  { 0x08030000, 0xF800 },
};
