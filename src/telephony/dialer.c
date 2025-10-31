#include "includes.h"

#include "screen.h"
#include "af.h"

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
	if (xx) glyph_num = lpeek(glyph_num_addr - 1);
	else glyph_num=0xa0;
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

void dialpad_draw(char active_field)
{
  uint8_t seq[12]={1,2,3,
		   4,5,6,
		   7,8,9,
		   11,0,10};

  // Draw GOTOX to right of dialpad, so that right display area
  // remains aligned.
  for(int y=2;y<MAX_ROWS;y++) draw_goto(40,y,40*8-1);

#define X_START 8
  int x = X_START;
  int y = 11;
  for(int d=0;d<=11;d++) {
    // Draw digits all in RED by default
    dialpad_draw_button(seq[d],x,y, (active_field==AF_DIALPAD)? 0x2e : 0x2b);  // 0x20 = reverse
    x+=6;
    if (x>(X_START+6+6)) { x=X_START; y+=5; }
  }

  y=11;
  // Call button : Green unless in a call
  dialpad_draw_button(12,2,y, 0x25);  // 0x20 = reverse
  // Mute button (only valid if not activated and not in a call
  dialpad_draw_button(14,2,y+5, 0x2b);  // 0x20 = reverse
  // Hang up button (only valid if in a call)
  dialpad_draw_button(13,2,y+5+5, 0x2b);  // 0x20 = reverse

  // Draw invisible button to make it all line up
  dialpad_draw_button(13,2,y+5+5+5, 0x06);

  
}
