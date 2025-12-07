#include "includes.h"

void loader_exec(char *progname)
{
  // Disable memory write protection
  lpoke(0xFFD5000L,0x00); 

  // Unhook IRQ
  asm volatile ("jsr $ff8a"); // Restore default KERNAL vectors

  // exec the other program
  mega65_cdroot();
  mega65_chdir("PHONE");

  mega65_dos_exechelper(progname);

  // Flag error
  fail(1);
  
  while(1) {
    POKE(0xD020,PEEK(0xD020)+1);
  }
}
