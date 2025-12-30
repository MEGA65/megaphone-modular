/*
  Simple minicom like terminal program for the MEGA65

*/

#include "includes.h"

#include "uart.h"

void print_text80(unsigned char x, unsigned char y, unsigned char colour, char *msg)
{
  uint16_t char_addr = 0xC000 + x + y * 80;
  while (*msg) {
    uint8_t char_code = *msg;
    if (*msg >= 0xc0 && *msg <= 0xe0)
      char_code = *msg - 0x80;
    else if (*msg >= 0x40 && *msg <= 0x60)
      char_code = *msg - 0x40;
    else if (*msg >= 0x60 && *msg <= 0x7A)
      char_code = *msg - 0x20;
    POKE(char_addr + 0, char_code);
    lpoke(0xff80000L - 0xc000 + char_addr, colour);
    msg++;
    char_addr += 1;
  }
}

void graphics_clear_screen(void)
{
  lfill(0x40000L, 0, 32768L);
  lfill(0x48000L, 0, 32768L);
}

void graphics_clear_double_buffer(void)
{
  lfill(0x50000L, 0, 32768L);
  lfill(0x58000L, 0, 32768L);
}

#include "ascii-font.c"

void h640_text_mode(void)
{
  // lower case
  POKE(0xD018, 0x16);

  // Normal text mode
  POKE(0xD054, 0x00);
  // H640, V400, fast CPU, extended attributes
  POKE(0xD031, 0xE8);
  // Adjust D016 smooth scrolling for VIC-III H640 offset
  POKE(0xD016, 0xC9);
  // 80 chars per line logical screen layout
  POKE(0xD058, 80);
  POKE(0xD059, 80 / 256);
  // Draw 80 chars per row
  POKE(0xD05E, 80);
  // Put 4KB screen at $C000
  POKE(0xD060, 0x00);
  POKE(0xD061, 0xc0);
  POKE(0xD062, 0x00);

  // 50 lines of text
  POKE(0xD07B, 50);

  // Use our ASCII charset (set last to avoid hot reg problems)
  lcopy((uint16_t)&ascii_font[0],0xe000,4096);
  POKE(0xD068,0x00);
  POKE(0xD069,0xE0);
  
  lfill(0xc000, 0x20, 4000);
  // Clear colour RAM, while setting all chars to 4-bits per pixel
  lfill(0xff80000L, 0x0E, 4000);
  
}

char status_line[80]="";

int main(void)
{
  mega65_io_enable();
  
  h640_text_mode();

  
  
}
