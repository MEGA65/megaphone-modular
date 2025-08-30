#include <stdio.h>
#include <string.h>

#include "ascii.h"

#include "includes.h"

#include "records.h"
#include "screen.h"
#include "buffers.h"

unsigned long screen_ram = 0x12000;
unsigned long colour_ram = 0xff80800L;

char *font_files[NUM_FONTS]={"EmojiColour","EmojiMono", "Sans", "UI"};
struct shared_resource fonts[NUM_FONTS];
unsigned long required_flags = SHRES_FLAG_FONT | SHRES_FLAG_16x16 | SHRES_FLAG_UNICODE;

char screen_setup_fonts(void)
{
  char err=0;
  unsigned char i;
  // Open the fonts
  for(i=0;i<NUM_FONTS;i++) {
    if (shopen(font_files[i],7,&fonts[i])) {
      //      printf("ERROR: Failed to open font '%s'\n", font_files[i]);
      err++;
    }
  }
  return err;
}


void screen_setup(void)
{
  // Blue background, black border
  POKE(0xD020,0);
  POKE(0xD021,6);

  // H640 + fast CPU + VIC-III extended attributes
  POKE(0xD031,0xE0);  
  
  // 16-bit text mode, alpha compositor, 40MHz
  POKE(0xD054,0xC5);

  // BOLD = ALT PALETTE so that REVERSE works for FCM
  POKE(0xD053,PEEK(0xD053)|0x10);
  
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

char draw_string_nowrap(unsigned char x_glyph_start, unsigned char y_glyph_start, // Starting coordinates in glyphs
			unsigned char f, // font
			unsigned char colour, // colour
			unsigned char *utf8,		     // Number of pixels available for width
			unsigned int x_start_px,
			unsigned int x_pixels_viewport,
			// Number of glyphs available
			unsigned char x_glyphs_viewport,
			unsigned char *str_end,
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

  // Allow drawing of string segments
  if (str_end) {
    if (utf8>=str_end) return 0;
  }

  
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

    // Allow drawing of string segments
    if (str_end) {
      if (utf8>=str_end) {
	utf8=str_end;
	break;
      }
    }
    
  }

  if (glyphs_used) *glyphs_used = x;
  if (pixels_used) *pixels_used = pixels_wide;

  if (padP) {
    pad_string_viewport(x+ x_glyph_start, y_glyph_start, // Starting coordinates in glyphs
			colour,
		        x_pixels_viewport - pixels_wide,  // Pixels remaining in viewport
			x_glyphs_viewport, // Right hand glyph of view port
			x_start_px + x_pixels_viewport); // VIC-IV pixel column to point GOTOX to

  }
  
  // Return the number of bytes of the string that were consumed
  return utf8 - utf8_start;
}


/* Tunable “penalties” (smaller = more preferred) */
#define BREAK_FORBIDDEN   255  /* default: don't break here */
#define BREAK_IDEAL         0  /* best places to break */
#define BREAK_GOOD         10
#define BREAK_OKAY         20
#define BREAK_BAD         200  /* strongly discouraged but not impossible */


/* Common Unicode code points we care about */
#define CP_SPACE        0x0020L
#define CP_NBSP         0x00A0L
#define CP_HYPHEN       0x002DL
#define CP_SOFT_HYPHEN  0x00ADL
#define CP_ZWSP         0x200BL

/* Simple classifier: return break cost for breaking AFTER cp, possibly using next_cp */
unsigned char compute_break_cost(unsigned long cp,
				 unsigned long next_cp,
				 unsigned char prev_was_space)
{
  /* Disallow breaks after nothing */
  if (cp == 0) return BREAK_FORBIDDEN;

  /* Hard “don’t break” spots */
  if (cp == CP_NBSP) return BREAK_FORBIDDEN;

  /* Zero-width space explicitly allows a break */
  if (cp == CP_ZWSP) return BREAK_IDEAL;

  /* A normal space is an ideal break point. If there are multiple spaces,
     prefer breaking after the LAST one (handled by prev_was_space logic below). */
  if (cp == CP_SPACE) {
    /* If the next cp is also a space, discourage breaking here to prefer the last space in the run. */
    if (next_cp == CP_SPACE) return BREAK_BAD;
    return BREAK_IDEAL;
  }

  /* Soft hyphen: okay to break here (render a hyphen when breaking), otherwise ignore.
     We just mark it a good place; the actual rendering decision can be handled elsewhere. */
  if (cp == CP_SOFT_HYPHEN) return BREAK_GOOD;

  /* ASCII hyphen-minus: good place to break (will show the hyphen). */
  if (cp == CP_HYPHEN) return BREAK_OKAY;

  /* Lightly discourage breaking right after an opening punctuation if the next is space.
     (Keeps lines from ending on an opening paren, etc., unless we must.) */
  if (cp == '(' || cp == '[' || cp == '{' ||
      cp == 0x201C || cp == 0x201D ||   /* “ ” */
      cp == 0x2018 || cp == 0x2019)     /* ‘ ’ */
    {
      return BREAK_BAD;
    }

  /* Otherwise, default: don't break here. */
  return BREAK_BAD;
}

char calc_break_points(unsigned char *str,
		       int font,
		       unsigned int box_width_pixels,
		       unsigned int box_width_glyphs)
{
  char r;
  unsigned char *s;
  unsigned int w_px;
  unsigned int w_g;
  unsigned char *line_start;
  unsigned char *line_end;
  unsigned int ofs;
  unsigned int best_break_ofs;
  unsigned char best_break_cost;
  unsigned char *best_break_s;
  unsigned char *last_break_s;
  unsigned char break_required;
  unsigned int this_cost, underful_cost;
  unsigned char j;
  
  // Box must be wide enough to take single widest glyph
  if (box_width_pixels<32) return 4;
  if (box_width_glyphs<2) return 4;
  
  buffers_lock(LOCK_TEXTBOX);

  if (!str) return 3;
  if (!*str) return 3;
  
  r = string_render_analyse(str, font,
			    &buffers.textbox.len,
			    buffers.textbox.pixel_widths,
			    buffers.textbox.glyph_widths,
			    buffers.textbox.break_costs);
  lcopy((unsigned long)buffers.textbox.pixel_widths,0x18000L,RECORD_DATA_SIZE);
  lcopy((unsigned long)buffers.textbox.glyph_widths,0x19000L,RECORD_DATA_SIZE);
  lcopy((unsigned long)buffers.textbox.break_costs,0x1A000L,RECORD_DATA_SIZE);

  w_g=0;
  w_px=0;
  ofs=0;
  s = str;

  best_break_ofs=0;
  best_break_cost=0xff;
  best_break_s=s;
  last_break_s=s;
  
  line_start = s;
  line_end = s;

  if (r) return r;

  buffers.textbox.line_count=0;
  while(*s) {
    unsigned long cp = utf8_next_codepoint(&s);

    break_required=0;
    // Run out of space yet? (we keep one glyph spare for a final GOTOX to ensure alignment)
    if (w_g + buffers.textbox.glyph_widths[ofs] > (box_width_glyphs -1)) break_required=1;
    if (w_px + buffers.textbox.pixel_widths[ofs] > box_width_pixels) break_required=1;

    if (break_required) {
      // If a line break is required, record it, and look for next line.
      buffers.textbox.line_offsets_in_bytes[buffers.textbox.line_count++]=(best_break_s-last_break_s);

      if (!best_break_ofs) {
	return 5;
      }

      w_px=0;
      w_g=0;
      ofs=best_break_ofs;
      s=best_break_s;
      best_break_ofs=0;
      best_break_cost=0xff;
      last_break_s = best_break_s;
      best_break_s = str;
    }
    else
      {
	// Check if cost here is better than the previous cost
	
	w_g+= buffers.textbox.glyph_widths[ofs];
	w_px+= buffers.textbox.pixel_widths[ofs];
	
	// Get base cost
	this_cost = buffers.textbox.break_costs[ofs];
	// Then add a penalty for unused pixels.
	underful_cost = box_width_pixels - w_px;
	
	if (this_cost + underful_cost <= best_break_cost) {
	  best_break_cost = this_cost + underful_cost;
	  best_break_ofs = ofs + 1;
	  best_break_s = s;
	}
	
	ofs++;
      }
    
  }

  if (*last_break_s) {
    // Emit final line
    buffers.textbox.line_offsets_in_bytes[buffers.textbox.line_count++]=(s-last_break_s);
    
  }

  // Leave TEXTBOX buffer locked, because the caller presumably intends to use the result of our calculations.

  return 0;
}


char textbox_draw(unsigned char x_start,
		  unsigned char y_start,
		  unsigned int x_start_px,
		  unsigned int box_width_pixels,
		  unsigned int box_width_glyphs,
		  unsigned char font,
		  unsigned char colour,
		  unsigned char *str,
		  unsigned int first_row,
		  unsigned int last_row,
		  unsigned char padding)
{
  unsigned int ofs,j;
  
  // buffer_unlock(LOCK_TEXTBOX);

  ofs=0;
  if (first_row) {
    for(j=0;(j<buffers.textbox.line_count)&&(j<first_row);j++)
    ofs+=buffers.textbox.line_offsets_in_bytes[j];
  } else j=0;

  for(;(j<=last_row)&&(j<buffers.textbox.line_count);j++)
    {
      draw_string_nowrap(x_start,y_start,
			 font,
			 colour,
			 &str[ofs],
			 x_start_px,
			 box_width_pixels,
			 box_width_glyphs,
			 &str[ofs+buffers.textbox.line_offsets_in_bytes[j]],
			 VIEWPORT_PADDED,
			 NULL,
			 NULL);
      ofs+=buffers.textbox.line_offsets_in_bytes[j];

      y_start++;
    }

  return 0;
}

/* Returns 0 on success, non-zero on error. */
char string_render_analyse(unsigned char *str,
                           int font,
                           unsigned int *len,
                           unsigned char *pixel_widths, /* [RECORD_DATA_SIZE] */
                           unsigned char *glyph_widths, /* [RECORD_DATA_SIZE] */
                           unsigned int *break_costs   /* [RECORD_DATA_SIZE] */
                           )
{
  unsigned char *s = (unsigned char*)str;
  unsigned int o = 0;

  if (!str) return 1;
  
  /* To compute break-after cost, we sometimes want to peek the next codepoint. */
  while (*s && o < RECORD_DATA_SIZE) {
    unsigned char pixel_count = 0;
    unsigned char glyph_count = 0;

    /* Decode current cp and remember where we are for peeking. */
    unsigned char *save = s;
    unsigned long cp = utf8_next_codepoint(&s);

    /* Peek the next codepoint (without consuming original stream). */
    unsigned char *peek = s;
    unsigned long next_cp = *peek ? utf8_next_codepoint(&peek) : 0;

    /* Measure glyph/pixel widths for this cp */
    glyph_count = lookup_glyph(font, cp, &pixel_count, NULL);

    if (pixel_widths) pixel_widths[o] = pixel_count;
    if (glyph_widths) glyph_widths[o] = glyph_count;

    /* Compute break cost AFTER this cp */
    if (break_costs) {
      /* If this is the very first character, never prefer a break (useless). */
      unsigned char cost = compute_break_cost(cp, next_cp, /*prev_was_space=*/0);

      /* Never allow a break at position 0 (no-op line) */
      if (o == 0) cost = BREAK_FORBIDDEN;

      break_costs[o] = cost;
    }

    o++;    
  }

  if (len) *len = o;

  /* If we stopped because RECORD_DATA_SIZE was reached but string continues, signal truncation. */
  if (*s) return 2; /* truncated output */

  return 0;
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
  unsigned char reverse = colour & 0x80; // MSB of colour indicates reverse

  // Make reverse bit be in correct place for SEAM colour RAM byte 1 reverse flag
  if (reverse) reverse = 0x20;
  
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
  lpoke(colour_ram + row0_offset + 1, ((colour_ram_1[table_index]+colour+reverse) ));

  // Set the number of pixels wide
  if (pixels_used) *pixels_used = cached_glyph_flags[i]&0x1f;
  
  // And the 2nd column, if required
  if ((cached_glyph_flags[i]&0x1f)>8) {
    // Screen RAM
    lpoke(screen_ram + row0_offset + 2, ((i&0x3f)<<2) + 2 );
    lpoke(screen_ram + row0_offset + 3, screen_ram_1_right[table_index] + (i>>6) + 0x10);

    // Colour Ram
    lpoke(colour_ram + row0_offset + 2, colour_ram_0_right[table_index]);
    lpoke(colour_ram + row0_offset + 3, colour_ram_1[table_index]+colour+reverse);

    // Rendered as 2 chars wide
    return 2;
  }

  // Rendered as 1 char wide
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
