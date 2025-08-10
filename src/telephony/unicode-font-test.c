#include <stdio.h>
#include <string.h>

#include "ascii.h"

#include "mega65/hal.h"
#include "mega65/shres.h"
#include "mega65/memory.h"

#define NUM_FONTS 4
#define FONT_EMOJI_COLOUR 0
#define FONT_EMOJI_MONO 1
#define FONT_TEXT 2
#define FONT_UI 3
char *font_files[NUM_FONTS]={"EmojiColour","EmojiMono", "Sans", "UI"};
struct shared_resource fonts[NUM_FONTS];
unsigned long required_flags = SHRES_FLAG_FONT | SHRES_FLAG_16x16 | SHRES_FLAG_UNICODE;

unsigned char buffer[128];

unsigned char i;

unsigned long screen_ram = 0x12000;
unsigned long colour_ram = 0xff80800L;

#define RENDER_COLUMNS 128
#define MAX_ROWS 30

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

void draw_goto(int x,int y, int goto_pos);

void screen_setup(void)
{
  // Blue background, black border
  POKE(0xD020,0);
  POKE(0xD021,6);

  // H640 + fast CPU + VIC-III extended attributes
  POKE(0xD031,0xE0);  
  
  // 16-bit text mode, alpha compositor, 40MHz
  POKE(0xD054,0xC5);

  // PAL
  POKE(0xD06f,0x00);
  
  // Retract borders to be 1px
  POKE(0xD05C,0x3B);
  POKE(0xD048,0x41); POKE(0xD049,0x00);
  POKE(0xD04A,0x20); POKE(0xD04B,0x02);  

  // 90 columns wide (but with virtual line length of 256)
  // Advance 512 bytes per line
  POKE(0xD058,0x00); POKE(0xD059,0x02);
  // XXX -- We can display more than 128, but then we need to insert GOTOX tokens to prevent RRB wrap-around
  POKE(0xD05E,RENDER_COLUMNS); // display 255 
  
  // 30+1 rows (because row 0 of text glitches when using interlaced)
  POKE(0xD07B,30 - 1);
  
  // Chargen vertically centred for 30 rows, and at left-edge of 720px display
  // (We _could_ use all 800px horizontally on the phone display, but then we can't see it on VGA output for development/debugging)
  // We have a hidden first row of text that we don't show, to avoid glitching of top raster in first row of text when interlaced text mode is enabled.
  POKE(0xD04C,0x3B); POKE(0xD04D,0x00);
  POKE(0xD04E,0x41 - 16); POKE(0xD04F,0x00);
  
  // Double-height char mode
  POKE(0xD07A,0x10);

  // Colour RAM offset
  POKE(0xD064,colour_ram>>0);
  POKE(0xD065,colour_ram>>8);
  
  // Screen RAM address
  POKE(0xD060,((unsigned long)screen_ram)>>0);
  POKE(0xD061,((unsigned long)screen_ram)>>8);
  POKE(0xD062,((unsigned long)screen_ram)>>16);

}

unsigned char nybl_swap(unsigned char v) {
    return ((v & 0x0F) << 4) | ((v & 0xF0) >> 4);
}

void generate_rgb332_palette(void)
{
  unsigned int i;
  
  // Select Palette 1 for access at $D100-$D3FF
  POKE(0xD070,0x40);
  
  for (i = 0; i < 256; i++) {
    // RGB332 bit layout:
    // Bits 7-5: Red (3 bits)
    // Bits 4-2: Green (3 bits)
    // Bits 1-0: Blue (2 bits)
    
    // Extract components and scale to 8-bit
    unsigned char red   = ((i >> 5) & 0x07) * 255 / 7;
    unsigned char green = ((i >> 2) & 0x07) * 255 / 7;
    unsigned char blue  = ( i       & 0x03) * 255 / 3;
    
    // Nybl-swap each value
    unsigned char red_swapped   = nybl_swap(red);
    unsigned char green_swapped = nybl_swap(green);
    unsigned char blue_swapped  = nybl_swap(blue);
    
    // Write to MEGA65 palette memory
    POKE(0xD100L + i, red_swapped);
    POKE(0xD200L + i, green_swapped);
    POKE(0xD300L + i, blue_swapped);
  }

  // Select Palette 0 for access at $D100-$D3FF,
  // use Palette 1 (set above) for alternate colour palette
  // (this is what we will use for unicode colour glyphs)
  POKE(0xD070,0x01);

}


// 128KB buffer for 128KB / 256 bytes per glyph = 512 unique unicode glyphs on screen at once
#define GLYPH_DATA_START 0x40000
#define GLYPH_CACHE_SIZE 512
#define BYTES_PER_GLYPH 256
unsigned long cached_codepoints[GLYPH_CACHE_SIZE];
unsigned char cached_fontnums[GLYPH_CACHE_SIZE];
unsigned char cached_glyph_flags[GLYPH_CACHE_SIZE];
unsigned char glyph_buffer[BYTES_PER_GLYPH];

void reset_glyph_cache(void)
{
  lfill(GLYPH_DATA_START,0x00,GLYPH_CACHE_SIZE * BYTES_PER_GLYPH);
  lfill((unsigned long)cached_codepoints,0x00,GLYPH_CACHE_SIZE*sizeof(unsigned long));
}

void load_glyph(int font, unsigned long codepoint, unsigned int cache_slot)
{
  unsigned char glyph_flags;

#if 0
  // XXX DEBUG show gradiant glyph
  for(glyph_flags=0;glyph_flags<255;glyph_flags++) glyph_buffer[glyph_flags]=glyph_flags;
  glyph_buffer[0xff]=codepoint&0x1f;
#else
  // Seek to and load glyph from font in shared resources
  shseek(&fonts[font],codepoint<<8,SEEK_SET);
  shread(glyph_buffer,256,&fonts[font]);
#endif

  // Extract glyph flags
  glyph_flags = glyph_buffer[0xff];

  // Replace glyph flag byte with indicated pixel value
  switch((glyph_flags>>5)&0x03) {
  case 0: glyph_buffer[0xff]=glyph_buffer[0xfe];
  case 1: glyph_buffer[0xff]=glyph_buffer[0x7f];
  case 2: glyph_buffer[0xff]=0;
  case 3: glyph_buffer[0xff]=0xff;
  }

  // Store glyph in the cache
#define FONT_CARD_ORDER_FIXED
#ifdef FONT_CARD_ORDER_FIXED
  lcopy((unsigned long)glyph_buffer,GLYPH_DATA_START + ((unsigned long)cache_slot<<8), BYTES_PER_GLYPH);
#else
  lcopy((unsigned long)glyph_buffer,GLYPH_DATA_START + ((unsigned long)cache_slot<<8), 64);
  lcopy((unsigned long)glyph_buffer + 64 ,GLYPH_DATA_START + ((unsigned long)cache_slot<<8) + 128, 64);
  lcopy((unsigned long)glyph_buffer + 128,GLYPH_DATA_START + ((unsigned long)cache_slot<<8) + 64, 64);
  lcopy((unsigned long)glyph_buffer + 192,GLYPH_DATA_START + ((unsigned long)cache_slot<<8)+ 192, 64);
#endif
  cached_codepoints[cache_slot]=codepoint;
  cached_fontnums[cache_slot]=font;
  cached_glyph_flags[cache_slot]=glyph_flags;
}

extern unsigned char screen_ram_1_left[64];
extern unsigned char screen_ram_1_right[64];
extern unsigned char colour_ram_0_left[64];
extern unsigned char colour_ram_0_right[64];
extern unsigned char colour_ram_1[64];

unsigned int row0_offset;

void draw_goto(int x,int y, int goto_pos)
{
  // Get offset within screen and colour RAM for both rows of chars
  row0_offset = (y<<9) + (x<<1);

  lpoke(screen_ram + row0_offset + 0, goto_pos);
  lpoke(screen_ram + row0_offset + 1, 0x00 + ((goto_pos>>8)&0x3));
  lpoke(colour_ram + row0_offset + 0, 0x10);
  lpoke(colour_ram + row0_offset + 1, 0x00);
  
}

void screen_clear(void)
{
  unsigned char y;

  // Clear screen RAM using overlapping DMA  
  lpoke(screen_ram + 0,0x20);
  lpoke(screen_ram + 1,0x00);
  lpoke(screen_ram + 2,0x20);
  lpoke(screen_ram + 3,0x00);
  lcopy(screen_ram, screen_ram + 4,(256*31*2) - 4);

  // Clear colour RAM
  lfill(colour_ram,0x01,(256*31*2));

  // Fill off-screen with GOTOX's to right edge, so that we don't overflow the 1024px RRB size
  for(y=0;y<30;y++) {
    // Draw one GOTOX
    draw_goto(RENDER_COLUMNS - 1,y,720);
  }
  
}


unsigned char lookup_glyph(int font, unsigned long codepoint,unsigned char *pixels_used, unsigned int *glyph_id)
{
  unsigned int i;

  for(i=0;i<GLYPH_CACHE_SIZE;i++) {
    if (!cached_codepoints[i]) break;
    if (cached_codepoints[i]==codepoint&&cached_fontnums[i]==font) break;
  }

  if (i==GLYPH_CACHE_SIZE) {
    // Cache full! We cannot draw this glyph.
    // XXX - Consider cache purging mechanisms, e.g., checking if all
    // glyphs in the cache are currently still on screen?
    // Should we reserve an entry in the cache slot for "unprintable glyph"?
    // (maybe just show it as [+HEX] for now? But that would be up to the calling
    // function to decide).
    return 0;
  }

  if (cached_codepoints[i]!=codepoint) {
    load_glyph(font, codepoint, i);
  } 

  if (pixels_used) *pixels_used = cached_glyph_flags[i]&0x1f;

  if (glyph_id) *glyph_id = i;
  
  // How many glyphs does it use?
  if (cached_glyph_flags[i]>8) return 2; else return 1;
}

char draw_glyph(int x, int y, int font, unsigned long codepoint,unsigned char colour, unsigned char *pixels_used)
{
  unsigned int i = 0x7fff;
  unsigned char table_index; 

  colour &= 0x0f;

  lookup_glyph(font, codepoint, NULL, &i);
  if (i==0x7fff) {
    // Could not find glyph
    return 0;
  }
  
  /*
    Draw the glyph in the indicated position in the screen RAM.
    Note that it is not the actual pixel coordinates on the screen.
    (Recall that we are using GOTOX after drawing the base layer of the screen, to then
     draw the variable-width parts of the interface over the top.)

    What we do do here, though, is set the glyph width bits in the screen RAM.
    We want this routine to be as fast as possible, so we maintain a cache of the colour RAM
    byte values based on every possible glyph_flags byte value.
    
  */

  // XXX -- Actually set the character pointers here

  // Construct 6-bit table index entry
  table_index = cached_glyph_flags[i]&0x1f;
  table_index |= (cached_glyph_flags[i]>>2)&0x20;

  // Get offset within screen and colour RAM for both rows of chars
  row0_offset = (y<<9) + (x<<1);

  // Set screen RAM
  lpoke(screen_ram + row0_offset + 0, ((i&0x3f)<<2) + 0 );
  lpoke(screen_ram + row0_offset + 1, screen_ram_1_left[table_index] + (i>>6) + 0x10);

  // Set colour RAM
  lpoke(colour_ram + row0_offset + 0, colour_ram_0_left[table_index]);
  lpoke(colour_ram + row0_offset + 1, colour_ram_1[table_index]+colour);

  // Set the number of pixels wide
  if (pixels_used) *pixels_used = cached_glyph_flags[i]&0x1f;
  
  // And the 2nd column, if required
  if ((cached_glyph_flags[i]&0x1f)>8) {
    // Screen RAM
    lpoke(screen_ram + row0_offset + 2, ((i&0x3f)<<2) + 2 );
    lpoke(screen_ram + row0_offset + 3, screen_ram_1_right[table_index] + (i>>6) + 0x10);

    // Colour Ram
    lpoke(colour_ram + row0_offset + 2, colour_ram_0_right[table_index]);
    lpoke(colour_ram + row0_offset + 3, colour_ram_1[table_index]+colour);

    // Rendered as 2 chars wide
    // XXX also report number of pixels consumed
    return 2;
  }

  // Rendered as 1 char wide
  // XXX also report number of pixels consumed
  return 1;
}

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

char draw_string_nowrap(unsigned char x_glyph_start, unsigned char y_glyph_start, // Starting coordinates in glyphs
			unsigned char f, // font
			unsigned char colour, // colour
			unsigned char *utf8,		     // Number of pixels available for width
			unsigned int x_pixels_viewport,
			// Number of glyphs available
			unsigned char x_glyphs_viewport,
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
  
  if (pixels_used) *pixels_used = 0;
  
  while (cp = utf8_next_codepoint(&utf8)) {
    // unsigned char f = pick_font_by_codepoint(cp);

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

char msg[80];

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

  // Open the fonts
  for(i=0;i<NUM_FONTS;i++) {
    if (shopen(font_files[i],7,&fonts[i])) {
      printf("ERROR: Failed to open font '%s'\n", font_files[i]);
    return;
    }
  }

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
      x += draw_glyph(x, 4, f, cp, 0x01,NULL);
    }
  }

  {
    unsigned int pixels_used = 0;
    unsigned char glyphs_used = 0;
    draw_string_nowrap(0,8, // Starting coordinates
		       FONT_UI, // font
		       0x01, // colour
		       "Hello world",
		       // Number of pixels available for width
		       720,
		       // Number of glyphs available
		       128,
		       // And return the number of each consumed
		       &pixels_used, &glyphs_used);

      msg[o++]='(';
      num_to_str(pixels_used,&msg[o]); o=strlen(msg);
      sprintf(&msg[o]," px, "); o=strlen(msg);
      num_to_str(glyphs_used,&msg[o]); o=strlen(msg);
      sprintf(&msg[o]," glyphs)"); o=strlen(msg);
      draw_string_nowrap(0,9, // Starting coordinates
			 FONT_UI, // font
			 0x0e, // colour
			 msg,
			 // Number of pixels available for width
			 720,
			 // Number of glyphs available
			 128,
			 NULL,NULL);


  }
  
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
