#include <stdio.h>
#include <string.h>

#include "ascii.h"

#include "mega65/hal.h"
#include "mega65/shres.h"
#include "mega65/memory.h"

#include "screen.h"

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



unsigned long utf8_next_codepoint(unsigned char **s)
{
  unsigned char *p;
  unsigned long cp;

  if (!s || !(*s)) return 0L;

  p = *s;
  
  if (p[0] < 0x80) {
    cp = p[0];
    (*s)++;
    return cp;
  }

  // 2-byte sequence: 110xxxxx 10xxxxxx
  if ((p[0] & 0xE0) == 0xC0) {
    if ((p[1] & 0xC0) != 0x80) return 0xFFFDL; // invalid continuation
    cp = ((p[0] & 0x1F) << 6) | (p[1] & 0x3F);
    *s += 2;
    return cp;
  }

  // 3-byte sequence: 1110xxxx 10xxxxxx 10xxxxxx
  if ((p[0] & 0xF0) == 0xE0) {
    if ((p[1] & 0xC0) != 0x80 || (p[2] & 0xC0) != 0x80) return 0xFFFDL;
    cp = ((p[0] & 0x0F) << 12) |
         ((p[1] & 0x3F) << 6) |
         (p[2] & 0x3F);
    *s += 3;
    return cp;
  }

  // 4-byte sequence: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
  if ((p[0] & 0xF8) == 0xF0) {
    if ((p[1] & 0xC0) != 0x80 || (p[2] & 0xC0) != 0x80 || (p[3] & 0xC0) != 0x80)
      return 0xFFFDL;
    cp = ((unsigned long)(p[0] & 0x07) << 18) |
      ((unsigned long)(p[1] & 0x3F) << 12) |
      ((p[2] & 0x3F) << 6) |
      (p[3] & 0x3F);
    *s += 4;
    return cp;
  }

  // Invalid or unsupported UTF-8 byte
  (*s)++;
  return 0xFFFDL;
}

char pick_font_by_codepoint(unsigned long cp)
{
    // Common emoji ranges
    if ((cp >= 0x1F300 && cp <= 0x1F5FF) ||  // Misc Symbols and Pictographs
        (cp >= 0x1F600 && cp <= 0x1F64F) ||  // Emoticons
        (cp >= 0x1F680 && cp <= 0x1F6FF) ||  // Transport & Map Symbols
        (cp >= 0x1F700 && cp <= 0x1F77F) ||  // Alchemical Symbols
        (cp >= 0x1F780 && cp <= 0x1F7FF) ||  // Geometric Extended
        (cp >= 0x1F800 && cp <= 0x1F8FF) ||  // Supplemental Arrows-C (used for emoji components)
        (cp >= 0x1F900 && cp <= 0x1F9FF) ||  // Supplemental Symbols and Pictographs
        (cp >= 0x1FA00 && cp <= 0x1FA6F) ||  // Symbols and Pictographs Extended-A
        (cp >= 0x1FA70 && cp <= 0x1FAFF) ||  // Symbols and Pictographs Extended-B
        (cp >= 0x2600 && cp <= 0x26FF)   ||  // Misc symbols (some emoji-like)
        (cp >= 0x2700 && cp <= 0x27BF)   ||  // Dingbats
        (cp >= 0xFE00 && cp <= 0xFE0F)   ||  // Variation Selectors (used with emoji)
        (cp >= 0x1F1E6 && cp <= 0x1F1FF))    // Regional Indicator Symbols (🇦 – 🇿)
        return FONT_EMOJI_COLOUR;    
    
    return FONT_TEXT;
}

char hex(unsigned char c)
{
  c&=0xf;
  if (c<0xa) return '0'+c;
  else return 'A'+c-10;
}

char pad_string_viewport(unsigned char x_glyph_start, unsigned char y_glyph, // Starting coordinates in glyphs
			 unsigned char colour,
			 unsigned int x_pixels_viewport,
			 unsigned char x_glyphs_viewport,
			 unsigned int x_viewport_absolute_end_pixel)
{
  unsigned int x = 0;
  unsigned int x_g = x_glyph_start;
  unsigned char px, trim;
  unsigned char reverse = 0;
  unsigned int i = 0;
  unsigned int row0_offset = 0;
  if (colour&0x80) reverse = 0x20;
  colour &= 0xf;

  // Lookup space character from UI font -- doesn't actually matter which font is active right now,
  // because we just need a blank glyph.
  lookup_glyph(FONT_UI,' ',NULL,&i);  

  while(x<x_pixels_viewport) {
    // How many pixels for this glyph
    px=16;
    if (px>(x_pixels_viewport-x)) px=x_pixels_viewport-x;

    trim = 16 - px;
    
    // Ran out of glyphs to make the alignment
    if (x_g==x_glyphs_viewport) return 1;

    // Get offset within screen and colour RAM for both rows of chars
    row0_offset = (y_glyph<<9) + (x_g<<1);
    
    // Set screen RAM
    // (Add $10 to make char data base = $40000)
    lpoke(screen_ram + row0_offset + 0, ((i&0x3f)<<2) + 0 );
    lpoke(screen_ram + row0_offset + 1, (trim<<5) + 0x10 + (i>>6));

    // Set colour RAM
    lpoke(colour_ram + row0_offset + 0, 0x08 + ((trim&8)>>1)); // NCM so we can do upto 16px per glyph
    lpoke(colour_ram + row0_offset + 1, colour+reverse);

    
    x_g++;
    x+=px;
  }

  // Write GOTOX to use up remainder of view port glyphs
  while(x_g<x_glyphs_viewport) {

    // Get offset within screen and colour RAM for both rows of chars
    row0_offset = (y_glyph<<9) + (x_g<<1);

    // Set screen RAM
    lpoke(screen_ram + row0_offset + 0, x_viewport_absolute_end_pixel&0xff);
    lpoke(screen_ram + row0_offset + 1, (x_viewport_absolute_end_pixel>>8)&0x3);

    // Set colour RAM
    lpoke(colour_ram + row0_offset + 0, 0x10);  // GOTOX flag
    lpoke(colour_ram + row0_offset + 1, 0x00);
        
    x_g++;
  }
  
  return 0;
}

#define VIEWPORT_PADDED 1
#define VIEWPORT_UNPADDED 0

char draw_string_nowrap(unsigned char x_glyph_start, unsigned char y_glyph_start, // Starting coordinates in glyphs
			unsigned char f, // font
			unsigned char colour, // colour
			unsigned char *utf8,		     // Number of pixels available for width
			unsigned int x_pixels_viewport,
			// Number of glyphs available
			unsigned char x_glyphs_viewport,
			unsigned char padP,
		     // And return the number of each consumed
			unsigned int *pixels_used,
			unsigned char *glyphs_used)
{
  unsigned char x=0;
  unsigned long cp;
  unsigned char *utf8_start = utf8;
  unsigned int pixels_wide = 0;
  unsigned char glyph_pixels;
  unsigned char n=0;
  unsigned char ff;
  
  if (pixels_used) *pixels_used = 0;
  
  while (cp = utf8_next_codepoint(&utf8)) {
    // Fall-back to emoji font when required if using the UI font
    if (f==FONT_UI) ff = pick_font_by_codepoint(cp);
    
    // Abort if the glyph won't fit.
    if (lookup_glyph(f,cp,&glyph_pixels, NULL) + x >= x_glyphs_viewport) break;
    if (glyph_pixels + pixels_wide > x_pixels_viewport) break;

    // Glyph fits, so draw it, and update our dimension trackers
    glyph_pixels = 0;
    x += draw_glyph(x_glyph_start + x, y_glyph_start, f, cp, colour, &glyph_pixels);
    pixels_wide += glyph_pixels;    
    }    

  if (glyphs_used) *glyphs_used = x;
  if (pixels_used) *pixels_used = pixels_wide;

  if (padP) {
    pad_string_viewport(x+ x_glyph_start, y_glyph_start, // Starting coordinates in glyphs
			colour,
		        x_pixels_viewport - pixels_wide,  // Pixels remaining in viewport
			x_glyphs_viewport-x, // Number of glyphs remaining in viewport
			x_pixels_viewport); // VIC-IV pixel column to point GOTOX to

  }
  
  // Return the number of bytes of the string that were consumed
  return utf8 - utf8_start;
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
      draw_glyph(x, 5, f, cp, 0x81,NULL);
      x += draw_glyph(x, 4, f, cp, 0x01,NULL);
    }
  }

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

#if 1
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
