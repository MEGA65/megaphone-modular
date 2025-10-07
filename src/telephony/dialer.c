#include "includes.h"

#include "screen.h"

// XXX Use the fact that chip RAM at 0x60000 reads as zeroes :)
#define DIALPAD_BLANK_GLYPH_ADDR 0x60000

void dialpad_draw_button(unsigned char symbol_num,
			 unsigned char x, unsigned char y,
			 unsigned char colour)
{
  unsigned long screen_ram_addr = screen_ram + y * 0x200 + x*2;
  unsigned long colour_ram_addr = colour_ram + y * 0x200 + x*2;

  unsigned long glyph_num_addr = 0x10000L + symbol_num * 8;

  unsigned int glyph_num = DIALPAD_BLANK_GLYPH_ADDR / 64;
  
  for(uint8_t yy=0;yy<4;yy++) {
    for(uint8_t xx=0;xx<4;xx++) {

      if (yy==0||yy==3) glyph_num = DIALPAD_BLANK_GLYPH_ADDR / 64;
      else {
	glyph_num = lpeek(glyph_num_addr);
	if (glyph_num!=0xa0) {
	  glyph_num = glyph_num*2 + (0x10080 / 64);
	} else glyph_num = DIALPAD_BLANK_GLYPH_ADDR / 64;
	glyph_num_addr++;
      }
      
      lpoke(screen_ram_addr + 0, glyph_num & 0xff);
      lpoke(screen_ram_addr + 1, glyph_num >>8);
      lpoke(colour_ram_addr + 0, 0x28);
      lpoke(colour_ram_addr + 1, colour);
      
      screen_ram_addr += 2;
      colour_ram_addr += 2;
    }
    
    screen_ram_addr += 0x200 - 4*2;  colour_ram_addr += 0x200 - 4*2;
  }
  
}

void dialpad_draw(void)
{
  uint8_t seq[12]={1,2,3,4,5,6,7,8,9,11,0,10};

  // Draw GOTOX to right of dialpad, so that right display area
  // remains aligned.
  for(int y=2;y<MAX_ROWS;y++) draw_goto(38,y,(38*8));
  
  int x = 5;
  int y = 5;
  for(int d=0;d<=11;d++) {
    // Draw digits all in RED by default
    dialpad_draw_button(seq[d],x,y, 0x22);  // 0x20 = reverse
    x+=6;
    if (x>(5+6+6)) { x=5; y+=5; }
  }

  
}
