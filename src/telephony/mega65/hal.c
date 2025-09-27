#include "includes.h"

extern const unsigned char __stack; 

unsigned char mountd81disk0(char *filename);
unsigned char mountd81disk1(char *filename);

void hal_init(void) {
}

char to_hex(unsigned char v)
{
  v&=0xf;
  if (v<0xa) return v+'0';
  if (v>0xf) return 0;
  return 'A'+(v-0xa);
}

unsigned char de_bcd(unsigned char in)
{
  return (in &0xf) + (in>>4)*10;  
}

unsigned long mega65_bcddate(void)
{
  // Format is 32-bit packed time.

  // Naive would be:
  // YEAR  = 16 bits BCD!
  // MONTH = 8 bits BCD!
  // DAY   = 8 bits BCD!

  unsigned int year;
  unsigned char month;
  unsigned char day;

  year = 0x2000 + lpeek(0xffd7115L);
  month = lpeek(0xffd7114L);
  day = lpeek(0xffd7113L);

  return (((unsigned long)year)<<16) + (month << 8) + day;
}

unsigned long mega65_bcdtime(void)
{
  // Format is 32-bit BCD packed time (24 hour time)
  return + (((unsigned long)(lpeek(0xffd7112L)&0x7f))<<16) + (lpeek(0xffd7111L)<<8) + lpeek(0xffd7110L);
}


char write_sector(unsigned char drive_id, unsigned char track, unsigned char sector)
{

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

  // Buffered sector write
  POKE(0xD081L,0x80);

  // Wait for BUSY to clear.
  // if RNF set, then it failed. If clear, then it was fine.
  while(PEEK(0xD082L) & 0x80) continue;

  if (PEEK(0xD082L) & 0x10) return 1;
  
  return 0;
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

  // Buffered sector read
  POKE(0xD081L,0x40);

  // Wait for BUSY to clear.
  // if RNF set, then it failed. If clear, then it was fine.
  while(PEEK(0xD082L) & 0x80) continue;

  if (PEEK(0xD082L) & 0x10) return 1;

  // Sector read correctly, and is in the sector buffer,
  // which is where SECTOR_BUFFER_ADDRESS points.
  return 0;
}

char mount_d81(char *filename, unsigned char drive_id)
{
  unsigned char r;
  switch(drive_id) {
  case 0: r=mountd81disk0(filename); break;
  case 1: r=mountd81disk1(filename); break;
  default: return 1;
  }

  if (r==1) {
    return 0;
  } else {
    return 2;
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

void mega65_uart_print(const char *s)
{  
  while(*s) {
    asm volatile (
        "sta $D643\n\t"   // write A to the trap register
        "clv"             // must be the very next instruction
        :
        : "a"(*s) // put 'error_code' into A before the block
        : "v", "memory"   // CLV changes V; 'memory' blocks reordering across the I/O write
    );

    // Wait a bit between chars
    for(char n=0;n<2;n++) {
      asm volatile(
		   "ldx $D012\n"
		   "1:\n"
		   "cpx $D012\n"
		   "beq 1b\n"
		   :
		   :
		   : "x"   // X is clobbered
		   );
    }
    
    s++;
  }

}

void mega65_uart_printhex(const unsigned char v)
{
  char hex_str[3];

  hex_str[0]=to_hex(v>>4);
  hex_str[1]=to_hex(v&0xf);
  hex_str[2]=0;
  mega65_uart_print(&hex_str[0]);
}

void mega65_fail(const char *file, const char *function, const char *line, unsigned char error_code)
{

  POKE(0x0428,PEEK(0x02));
  POKE(0x0429,PEEK(0x03));

  mega65_uart_print(file);

  mega65_uart_print(":");

  mega65_uart_print(line);
  mega65_uart_print(":");
  mega65_uart_print(function);
  mega65_uart_print("():0x");

  mega65_uart_printhex(error_code);
  mega65_uart_print("\n\r");


  while(PEEK(0xD610)) POKE(0xD610,0);
  while(!PEEK(0xD610)) POKE(0xD021,PEEK(0xD012));

}
