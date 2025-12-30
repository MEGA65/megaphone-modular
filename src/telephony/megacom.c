/*
  Simple minicom like terminal program for the MEGA65

*/

#include "includes.h"

#include "uart.h"

void screen_stash(void)
{
  lcopy(0xf000,0x40000L,80*50);
  lcopy(0xff80000L,0x40000L+80*50,80*50);
}

void screen_restore(void)
{
  lcopy(0x40000L,0xf000,80*50);
  lcopy(0x40000L+80*50,0xff80000L,80*50);
}

void print_box(unsigned char x1, unsigned char y1,
	       unsigned char x2, unsigned char y2,
	       unsigned char colour)
{
  uint16_t char_addr = 0xf000 + y1 * 80;
  for(int x=x1;x<=x2;x++) {
    POKE(char_addr+x,0x20);
    lpoke(0xff80000L - 0xf000 + char_addr+x, 0x20 | colour);    
  }
  for(int y=y1+1;y<y2;y++) {
    char_addr+=80;
    POKE(char_addr+x1,0x20);
    lpoke(0xff80000L - 0xf000 + char_addr+x1, 0x20 | colour);    
    POKE(char_addr+x2,0x20);
    lpoke(0xff80000L - 0xf000 + char_addr+x2, 0x20 | colour);        
  }
  char_addr+=80;
  for(int x=x1;x<=x2;x++) {
    POKE(char_addr+x,0x20);
    lpoke(0xff80000L - 0xf000 + char_addr+x, 0x20 | colour);    
  }
  
}

void print_text80(unsigned char x, unsigned char y, unsigned char colour, char *msg)
{
  uint16_t char_addr = 0xf000 + x + y * 80;
  while (*msg) {
    uint8_t char_code = *msg;
    POKE(char_addr + 0, char_code);
    lpoke(0xff80000L - 0xf000 + char_addr, colour);
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
  // Put 4KB screen at $F000
  POKE(0xD060, 0x00);
  POKE(0xD061, 0xf0);
  POKE(0xD062, 0x00);

  // 50 lines of text
  POKE(0xD07B, 50);

  // Use our ASCII charset (set last to avoid hot reg problems)
  lcopy((uint16_t)&ascii_font[0],0xe000,4096);
  POKE(0xD068,0x00);
  POKE(0xD069,0xE0);
  
  
  lfill(0xf000, 0x20, 4000);
  // Clear colour RAM, while setting all chars to 4-bits per pixel
  lfill(0xff80000L, 0x0E, 4000);
  
}

void visual_bell(void)
{
  POKE(0xD021,0x01);
  for(int i=0;i<10;i++) {
    uint8_t old = PEEK(0xD7FA);
    while (PEEK(0xD7FA)==old) continue;
  }
  POKE(0xD021,0x06);
}

char status_line[80 +1]="CTRL-A Z for help |  0000000 8N1 | MEGAcom 0.1 | ASCII         | Buffered uart 0";
#define BAUD_OFFSET 22
#define UART_NUM_OFFSET 79

struct baud_rate {
  uint32_t baud;
  char baud_str[9];
  uint32_t baud_divisor;
};

#define NUM_BAUD_RATES 22
struct baud_rate baud_list[NUM_BAUD_RATES]={
  {300, "    300", 40500000 / 300},
  {1200,"   1200", 40500000 / 1200},
  {2400,"   2400", 40500000 / 2400},
  {4800,"   4800", 40500000 / 4800},
  {9600,"   9600", 40500000 / 9600},
  {19200,"  19200", 40500000 / 19200},
  {38400,"  38400", 40500000 / 38400},
  {57600,"  57600", 40500000 / 57600},
  {115200," 115200", 40500000 / 115200},
  {230400," 230400", 40500000 / 230400},
  {460800," 460800", 40500000 / 460800},
  {500000," 500000", 40500000 / 500000},
  {576000," 576000", 40500000 / 576000},
  {921600," 921600", 40500000 / 921600},
  {1000000,"1000000", 40500000 / 1000000},
  {1152000,"1152000", 40500000 / 1152000},
  {1500000,"1500000", 40500000 / 1500000},
  {2000000,"2000000", 40500000 / 2000000},
  {2500000,"2500000", 40500000 / 2500000},
  {3000000,"3000000", 40500000 / 3000000},
  {3500000,"3500000", 40500000 / 3500000},
  {4000000,"4000000", 40500000 / 4000000},
};

uint8_t current_baud_rate = 8;
uint8_t current_uart = 1;

#define SERIAL_PORT_MENU_ITEMS 16
char *serial_port_menu_item[SERIAL_PORT_MENU_ITEMS]={
" A -    Serial Device      : Buffered UART 0      ",
" B - Lockfile Location     : /var/lock            ",
" C -   Callin Program      :                      ",
" D -  Callout Program      :                      ",
" E -    Bps/Par/Bits       :      00 8N1          ",
" F - Hardware Flow Control : No                   ",
" G - Software Flow Control : No                   ",
" H -     RS485 Enable      : No                   ",
" I -   RS485 Rts On Send   : No                   ",
" J -  RS485 Rts After Send : No                   ",
" K -  RS485 Rx During Tx   : No                   ",
" L -  RS485 Terminate Bus  : No                   ",
" M - RS485 Delay Rts Before: 0                    ",
" N - RS485 Delay Rts After : 0                    ",
"                                                  ",
"    Change which setting?                         ",
};
  

void serial_port_menu(void)
{

  POKE(0xD610,0);
  
  while(1) {
    
    print_box(4,3,55,3+SERIAL_PORT_MENU_ITEMS+2,0x0c);
    for(int i=0;i<SERIAL_PORT_MENU_ITEMS;i++) {
      // Avoid tearing with baud rate display update
      while(PEEK(0xD012)<0x80) continue;

      print_text80(5,4+i,0x0e,serial_port_menu_item[i]);

      // Update current UART and baud rate -- we do this in here
      // so that there's less chance of visible tearing
      if (!i) {
	POKE(0xf000+4*80+48,'0'+current_uart);
	POKE(0xf000+49*80+79,'0'+current_uart);
      }
      if (i==4) {
	for(int i=0;i<7;i++) POKE(0xf000+8*80+34+i,baud_list[current_baud_rate].baud_str[i]);
	for(int i=0;i<7;i++) POKE(0xf000+49*80+21+i,baud_list[current_baud_rate].baud_str[i]);
      }
      
    }

    if (PEEK(0xD610)) {
      switch(PEEK(0xD610)) {
      case 0x1b: // ESC - Exit
	POKE(0xD610,0);
	screen_restore();
	return;
      case 'a':
	current_uart++;
	if (current_uart>7) current_uart=0;
	modem_setup_serial(current_uart,baud_list[current_baud_rate].baud_divisor);
	break;
      case 'e':
	current_baud_rate++;
	if (current_baud_rate>=NUM_BAUD_RATES) current_baud_rate=0;
	modem_setup_serial(current_uart,baud_list[current_baud_rate].baud_divisor);
      default:
	visual_bell();
      }
      POKE(0xD610,0);
    }
  }
  


}

#define CONFIG_MENU_ITEMS 9
char *config_item[CONFIG_MENU_ITEMS]={
" Filenames and paths     ",
  " File transfer protocols ",
  " Serial port setup       ",
  " Modem and dialing       ",
  " Screen                  ",
  " Keyboard and Misc       ",
  " Save setup as dfl       ",
  " Save setup as..         ",
  " Exit                    ",
};
   

void config_menu(void)
{
  screen_stash();

  uint8_t item = 0;

  while(1) {
    print_box(12,9,12+26,9+10,0x0c);
    for(int i=0;i<CONFIG_MENU_ITEMS;i++) {
      print_text80(13,10+i,(i==item)?0x2e:0x0e,config_item[i]);
    }
    
    if (PEEK(0xD610)) {
      switch(PEEK(0xD610)) {
      case 0x1b: // ESC - Exit
	screen_restore();
	return;
      case 0x11: // DOWN
	item++;
	if (item>=CONFIG_MENU_ITEMS) item=0;
	break;
      case 0x91: // UP
	item--;
	if (item>=CONFIG_MENU_ITEMS) item=CONFIG_MENU_ITEMS-1;
	break;
      case 0x0d: // Select item
	switch(item) {
	case 0: // Filenames and paths
	case 1: // File transfer protocols
	case 3: // Modem and dialing
	case 4: // Screen
	case 5: // Keyboard and Misc
	case 6: // Save setup as dfl
	case 7: // Save setup as..
	  // Unimplemented!
	  visual_bell();
	  break;
	case 2: // Serial port setup
	  serial_port_menu();
	  break;
	case 8: // Exit
	  screen_restore();
	  return;
	}
	break;
      }
      POKE(0xD610,0);
    }
  }
  
}

uint8_t term_colour=0x0e;
uint8_t term_x=0;
uint8_t term_y=0;
uint8_t term_esc=0;

void term_scroll_check(void)
{
  if (term_x>79) {
    term_x=0;
    term_y++;
  }

  if (term_y>=49) {
    // Scroll screen
    term_y=48;
    lcopy(0xf050,0xf000,48*80);
    lcopy(0xff80050L,0xff80000L,48*80);

    // Blank last line of screen after scroll
    lfill(0xf000+80*48,' ',80);
    lfill(0xff80000L+80*48,term_colour,80);
  }
  
}

void term_process_char(uint8_t c)
{
  if (term_esc) {
    // XXX - Actually implement ESC mode properly
    switch(term_esc) {
    case 1:
      if (c=='[') term_esc=2; else term_esc=0;
      break;
    case 2:
      if (c=='m') term_esc=0;
      break;
    default:
      term_esc=0;
    }
  } else {
    switch(c) {
    case 0x07:
      visual_bell();
      break;
    case 0x08: // back space
      term_x--;
      if (term_x>79) {term_x=79; term_y--; }
      if (term_y<0) term_y=0;
      POKE(0xf000+term_y*80+term_x,' ');      
      break;
    case 0x1b:
      term_esc=1;
      break;
    case 0x0d:
      term_x=0;
      break;
    case 0x0a:
      // Advance a line
      term_y++;
      term_scroll_check();
      break;
    default:
      POKE(0xf000 + term_y*80 + term_x, c);
      lpoke(0xff80000L + term_y*80 + term_x, term_colour);
      term_x++;
      term_scroll_check();
    }
  }
}

int main(void)
{
  mega65_io_enable();

  // Install NMI and BRK catchers
  POKE(0x0316,(uint8_t)(((uint16_t)&brk_catcher)>>0));
  POKE(0x0317,(uint8_t)(((uint16_t)&brk_catcher)>>8));
  POKE(0x0318,(uint8_t)(((uint16_t)&nmi_catcher)>>0));
  POKE(0x0319,(uint8_t)(((uint16_t)&nmi_catcher)>>8));
  
  h640_text_mode();

  print_text80(0,49,0x21,status_line);

  // Apply initial serial port settings
  modem_setup_serial(current_uart,baud_list[current_baud_rate].baud_divisor);	
  POKE(0xf000+49*80+79,'0'+current_uart);
  for(int i=0;i<7;i++) POKE(0xf000+49*80+21+i,baud_list[current_baud_rate].baud_str[i]);
  
  uint8_t ctrl_a_mode=0;

  unsigned char c;

  print_text80(0,45,0x0e,"Welcome to MEGAcom 0.1");
  term_y=47;
  term_x=0;
  
  while(1) {

    uint16_t read_count = modem_uart_read(&c,1);
    if (read_count) {
      // Got a char -- render it
      term_process_char(c);
    }
    
    if (PEEK(0xD610)) {
      if (ctrl_a_mode) {
	switch(PEEK(0xD610)) {
	case 0x01:
	  // Send CTRL-A
	  break;
	case 'o':
	  // Config menu
	  config_menu();
	  break;
	case 'q':
	  // Quit
	  break;
	default:
	}
	ctrl_a_mode=0;
      } else {
	switch(PEEK(0xD610)) {
	case 0x01: ctrl_a_mode=1; break;
	case 0x14: // INST/DEL -> send backspace
	  c=0x08;
	  modem_uart_write(&c,1);
	  break;
	default:
	  // Send char to UART
	  c=PEEK(0xD610);
	  modem_uart_write(&c,1);
	}
      }

      // Pop key from queue
      POKE(0xD610,0);
    }
  }
  
  print_box(10,10,12,12,1);


  
}
