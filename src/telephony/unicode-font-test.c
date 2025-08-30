#include <stdio.h>
#include <string.h>

#include "ascii.h"

#include "includes.h"

#include "buffers.h"
#include "screen.h"
#include "records.h"
#include "contacts.h"

unsigned char buffer[128];

unsigned char i;

struct text_box {
  // The UTF-8 text to be rendered
  unsigned char *text;
  // Text box position in characters
  unsigned char cx,cy;
  // Text box right most and lowest char position
  unsigned char cx2,cy2;
  // Width of the text box in pixels
  // Note: Does not have to match the natural width of the underlying characters.
  // Rather, it's quite normal for the width to be quite a bit narrower, to allow
  // for many trimmed characters
  unsigned int width;
  // The current position of rendering
  unsigned char cxpos;
  unsigned int xpos;
  unsigned char ypos;

  // Pointers to each line of text
  unsigned char rows;
  unsigned char *line_breaks[MAX_ROWS];
};



char hex(unsigned char c)
{
  c&=0xf;
  if (c<0xa) return '0'+c;
  else return 'A'+c-10;
}

char *num_to_str(unsigned int n,char *s)
{
  char *start = s;
  char active=0;
  char c;
  if (n>9999) {
    c='0';
    while(n>9999) { c++; n-=10000; }
    *s = c;
    s++;
    active=1;
  }
  if (n>999||active) {
    c='0';
    while(n>999) { c++; n-=1000; }
    *s = c;
    s++;
    active=1;
  }
  if (n>99||active) {
    c='0';
    while(n>99) { c++; n-=100; }
    *s = c;
    s++;
    active=1;
  }
  if (n>9||active) {
    c='0';
    while(n>9) { c++; n-=10; }
    *s = c;
    s++;
    active=1;
  }
  *s = '0'+n;
  s++;
  *s=0;

  return start;
}

unsigned char y;

void main(void)
{
  shared_resource_dir d;
  unsigned char o=0;

  mega65_io_enable();
  
  screen_setup();
  screen_clear();

  generate_rgb332_palette();
  
  // Make sure SD card is idle
  if (PEEK(0xD680)&0x03) {
    POKE(0xD680,0x00);
    POKE(0xD680,0x01);
    while(PEEK(0xD680)&0x3) continue;
    usleep(500000L);
  }

  screen_setup_fonts();
  
  // Say hello to the world!
  draw_glyph(0,1, FONT_UI, 'E',0x01,NULL);
  draw_glyph(1,1, FONT_UI, 'm',0x01,NULL);
  draw_glyph(3,1, FONT_UI, 'o',0x01,NULL);
  draw_glyph(4,1, FONT_UI, 'j',0x01,NULL);
  draw_glyph(5,1, FONT_UI, 'i',0x01,NULL);
  draw_glyph(6,1, FONT_UI, '!',0x01,NULL);
  draw_glyph(7,1, FONT_UI, ' ',0x01,NULL);
  draw_glyph(8,1, FONT_EMOJI_COLOUR, 0x1f929L,0x01,NULL);

  {
    unsigned char *string="Ümlaute! 👀 😀 😎 🐶 🐙 🍕 🍣 ⚽️ 🎮 🛠️ 🚀 🎲 🧩 📚 🧪 🎵 🎯 💡 🔥 🌈 🪁";
    unsigned char *s=string;
    unsigned char x=0;
    unsigned long cp;

    while (cp = utf8_next_codepoint(&s)) {
      unsigned char f = pick_font_by_codepoint(cp);
      draw_glyph(x, 5, f, cp, 0x8d,NULL);
      x += draw_glyph(x, 4, f, cp, 0x01,NULL);
    }
  }

#if 0
  draw_string_nowrap(0,8, // Starting coordinates
		     FONT_UI, // font
		     0x01, // colour
		     "  Hello world",
		     // Number of pixels available for width
		     120,
		     // Number of glyphs available
		     32,
		     VIEWPORT_PADDED,
		     // And don't return the number of each consumed
		     NULL,NULL);
  for(y=0;y<10;y++)
    draw_string_nowrap(0,9+y, // Starting coordinates
		       FONT_UI, // font
		       0x81, // colour
		       "  Hello world",
		       // Number of pixels available for width
		       120L,
		       // Number of glyphs available
		       32,
		       VIEWPORT_PADDED,
		       // And return the number of each consumed
		       NULL,NULL);
#endif

  buffers_lock(LOCK_TEXTBOX);
  
  // record = 0 BAM, 1 = unknown contact place-holder. 2 = first real contact
  mount_contact_qso(2);
  // record 0 = BAM, 1 = first actual message
  read_record_by_id(0,1,buffers.textbox.record);

  buffers.textbox.field = *find_field(buffers.textbox.record,
				      0,
				      FIELD_BODYTEXT,
				      &buffers.textbox.field_len);
  
  calc_break_points(buffers.textbox.field,
		    FONT_UI,
		    100, // px width
		    40   // glyph width
		    );
  
  buffers_unlock(LOCK_TEXTBOX);
  
#if 0
    {
      for(y=7;y<12;y++) {
	draw_string_nowrap(20,y, // Starting coordinates
			   FONT_UI, // font
			   0x01, // colour
			   "|",
			   // Number of pixels available for width
			   720 - 150,
			   // Number of glyphs available
			   32,
			   VIEWPORT_UNPADDED,
			   // And return the number of each consumed
			   NULL,NULL);
	
      }
    }
#endif
    
  while(1) continue;

#if 0
  // Try drawing a unicode glyph
  {
    unsigned long codepoint = 0x1f600L;
    while(1) {
      screen_clear();
      draw_glyph(0,1, FONT_EMOJI_COLOUR, codepoint,0x01,NULL);
      lpoke(screen_ram + 1024,codepoint&0x1f);
      while(PEEK(0xD610)) POKE(0xD610,0);
      while(!PEEK(0xD610)) continue;
      if (PEEK(0xD610)==',') codepoint--;
      if (PEEK(0xD610)=='.') codepoint++;
    }
  }
#endif
  
  return;
}
