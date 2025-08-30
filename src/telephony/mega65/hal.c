#include "includes.h"

unsigned char mountd81disk0(char *filename);
unsigned char mountd81disk1(char *filename);

void hal_init(void) {
}

char write_sector(unsigned char drive_id, unsigned char track, unsigned char sector)
{
  return 1;
}

char read_sector(unsigned char drive_id, unsigned char track, unsigned char sector)
{
  /*
    Simple FDC sector read routine.
    XXX - We should make sure we turn the motor on to work with real drives.
    But for now, we are just focussing on D81 disk images, on the basis that
    the telephony software _really_ isn't designed for you to operate your phone
    with a deck of floppies.
   */

  // Select FDC rather than SD card sector buffer
  POKE(0xD689L,PEEK(0xD689L)&0x7f);
  
  // Cancel any previous command
  POKE(0xD081L,0x00);

  // Reset FDC buffers
  POKE(0xD081L, 0x01);
  
  // Fail if BUSY flag already set
  if (PEEK(0xD082L) & 0x80) return 2;
  
  // Select and start drive 
  POKE(0xD080L,
       0x60 + // Motor, select
       ((sector>10)?0x00:0x08) // Side select based on sector number
       );
  
  // XXX If the motor was previously off, we should give it some time to spin up for real drives.
  // XXX Seek to the physical track for real drives.
  
  // Specify track, sector and side bytes for FDC to look for (note that physical
  // side selection happens above).

  // Physical tracks are 0-79 for logical tracks 1-80
  POKE(0xD084L,track-1);

  // Physical sector numbers start at 1, not 0, and are 1-10 on each side.
  if (sector>10) {
    POKE(0xD085L,1+sector-10);
    POKE(0xD086L,0x01); // reverse side
  } else {
    POKE(0xD085L,1+sector);
    POKE(0xD086L,0x00); // front side
  }

  POKE(0x0400,PEEK(0xD082L));
  
  // Buffered sector read
  POKE(0xD081L,0x40);

  POKE(0x0401,PEEK(0xD082L));
  
  // Wait for BUSY to clear.
  // if RNF set, then it failed. If clear, then it was fine.
  while(PEEK(0xD082L) & 0x80) continue;

  POKE(0x0402,PEEK(0xD082L));
  
  if (PEEK(0xD082L) & 0x10) return 1;

  // Sector read correctly, and is in the sector buffer,
  // which is where SECTOR_BUFFER_ADDRESS points.
  return 0;
}

char mount_d81(char *filename, unsigned char drive_id)
{
  switch(drive_id) {
  case 0: if(mountd81disk0(filename)==1) return 0; else return 2;
  case 1: if(mountd81disk1(filename)==1) return 0; else return 2;
  default: return 1;
  }
}

char create_d81(char *filename)
{
  return 1;
}

char mega65_mkdir(char *dir)
{
  return 1;
}

char mega65_cdroot(void)
{
  // XXX - Doesn't allow use of different partitions
  chdirroot(0);
  // XXX - chddirroot()'s HYPPO call lacks failure semantics, and doesn't set return value.
  // So we have to assume it succeeded.
  return 0;
}

char mega65_chdir(char *dir)
{
  if (chdir(dir)) return 1;
  return 0;
}

