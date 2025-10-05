
#ifndef INCLUDES_H
#define INCLUDES_H

#include "mega65/hal.h"
#include "mega65/shres.h"
#include "mega65/memory.h"
#include "mega65/fileio.h"

// 6E00 = SD card, 6C00 = FDC
#define SECTOR_BUFFER_ADDRESS 0xFFD6C00L

// Requires ROM writeable.  C64 KERNAL is at 0x2E000L, so we have 
#define WORK_BUFFER_SIZE (88*1024)
#define WORK_BUFFER_ADDRESS 0x18000L
// (Also note that 0x12000L is where the screen RAM is, and the glyph buffer is in 0x40000L-0x5FFFFL,
//  so we have to dodge all that.


void hal_init(void);
char write_sector(unsigned char drive_id, unsigned char track, unsigned char sector);
char read_sector(unsigned char drive_id, unsigned char track, unsigned char sector);
char mount_d81(char *filename, unsigned char drive_id);
char create_d81(char *filename);
char mega65_mkdir(char *dir);
char mega65_cdroot(void);
char mega65_chdir(char *dir);

unsigned long mega65_bcddate(void);
unsigned long mega65_bcdtime(void);

#define WITH_SECTOR_MARKERS 1
#define NO_SECTOR_MARKERS 0
void format_image_fully_allocated(char drive_id,char *header, char withSectorMarkers);

char sort_d81(char *name_in, char *name_out, unsigned char field_id);

void dump_sector_buffer(char *m);
void dump_bytes(char *msg, unsigned long addr, int len);

char to_hex(unsigned char v);

#ifdef WITH_BACKTRACE
#define STR_HELPER(x) #x
#define STR(x)        STR_HELPER(x)

#define fail(X) mega65_fail(__FILE__,__FUNCTION__,STR(__LINE__),X)
void mega65_fail(const char *file, const char *function, const char *line, unsigned char error_code);
#else
#define fail(X)
#endif

struct function_table {
  const uint16_t addr;
  const char *function;
};

void dump_backtrace(void);
void nmi_catcher(void);

#endif

extern unsigned char tof_r;
#define try_or_fail(X) if ((tof_r=X)!=0) fail(tof_r)
void mega65_uart_print(const char *s);
void mega65_uart_printhex(const unsigned char v);
void mega65_uart_printptr(const void *v);
void mega65_uart_printhex16(const uint16_t v);

#define CHECKPOINT(X) { mega65_uart_print(__FILE__); mega65_uart_print(":"); mega65_uart_printhex16(__LINE__); mega65_uart_print(":"); mega65_uart_print(__FUNCTION__); mega65_uart_print(":"); mega65_uart_print(X); mega65_uart_print("\n\r"); }
#define CHECKPOINT_WAIT(X) { CHECKPOINT(X); while(PEEK(0xD610)) POKE(0xD610,0); while(!PEEK(0xD610)) POKE(0xD020,PEEK(0xD020)+1); POKE(0xD610,0); }

