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

void main(void)
{
  shared_resource_dir d;
  unsigned char o=0;
  int y=28;
  unsigned int message_count = 0;

  
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

#if 0
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
#endif

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

  // Read BAM, find first free sector (if we don't write it back, it doesn't actually
  // get allocated, thus saving the need for a separate get allocated count function).
  read_sector(0,1,0);
  message_count = record_allocate_next(SECTOR_BUFFER_ADDRESS);
  message_count--; 

  lcopy(&message_count,0x12000L,2);

  y = MAX_ROWS;
  
  while(y>=2&&message_count>0) {

    unsigned int first_row = 0;
    unsigned char we_sent_it = 0;
    
    // Read the message
    read_record_by_id(0,message_count,buffers.textbox.record);

    // Get message direction
    buffers.textbox.field = find_field(buffers.textbox.record,
				       RECORD_DATA_SIZE,
				       FIELD_MESSAGE_DIRECTION,
				       &buffers.textbox.field_len);  
    if (buffers.textbox.field&&buffers.textbox.field[0]) we_sent_it=1;      
    
    // Get the message body
    buffers.textbox.field = find_field(buffers.textbox.record,
				       RECORD_DATA_SIZE,
				       FIELD_BODYTEXT,
				       &buffers.textbox.field_len);  

    // And figure out how many lines it will take on the screen.
    calc_break_points(buffers.textbox.field,
		      FONT_UI,
		      255, // px width
		      60   // glyph width
		      );

    // Adjust y to the necessary starting row.    
    y = y - buffers.textbox.line_count;
    first_row = 0;
    while(y<2) {
      y++;
      first_row++;
    }

    textbox_draw(we_sent_it? 384/8 : 360/8, // column on screen
		 y, // row on screen
		 we_sent_it? 384 : 360, // start pixel
		 255, // px width
		 RENDER_COLUMNS - 1 - (we_sent_it? 48 : 45),   // glyph width
		 FONT_UI,
		 we_sent_it ? 0x8D : 0x8F, // colour
		 buffers.textbox.field,
		 first_row, // Starting row of text box
		 buffers.textbox.line_count-1, // Ending row of text box
		 VIEWPORT_PADDED);

    // Leave blank line between messages
    y--;
    
    message_count--;
  }    

#if 0
  // record 0 = BAM, 1 = first actual message
  read_record_by_id(0,1,buffers.textbox.record);

  buffers.textbox.field = find_field(buffers.textbox.record,
				     RECORD_DATA_SIZE,
				     FIELD_BODYTEXT,
				     &buffers.textbox.field_len);  
  
  calc_break_points(buffers.textbox.field,
		    FONT_UI,
		    255, // px width
		    60   // glyph width
		    );

  textbox_draw(360/8, // column on screen
	       2, // row on screen
	       360, // start pixel
	       255, // px width
	       RENDER_COLUMNS - 1 - 45,   // glyph width
	       FONT_UI,
	       0x8F, // colour
	       buffers.textbox.field,
	       0, // Starting row of text box
	       buffers.textbox.line_count-1, // Ending row of text box
	       VIEWPORT_PADDED);

  textbox_draw(384/8, // 384/8, // column on screen
	       12, // row on screen
	       384, // start pixel
	       270, // px width
	       RENDER_COLUMNS - 1 - 48,   // glyph width
	       FONT_UI,
	       0x8D, // colour
	       buffers.textbox.field,
	       0, // Starting row of text box
	       buffers.textbox.line_count-1, // Ending row of text box
	       VIEWPORT_PADDED);
#endif  

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
