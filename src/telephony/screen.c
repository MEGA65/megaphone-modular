#include <stdio.h>
#include <string.h>

#include "ascii.h"

#include "includes.h"

#include "records.h"
#include "screen.h"
#include "buffers.h"
#include "shstate.h"

// $10000-$127FF is reserved for dial pad glyphs
unsigned long screen_ram = 0x12800;
unsigned long colour_ram = 0xff80800L;

char *font_files[NUM_FONTS]={"EmojiColour","EmojiMono", "Sans", "UI"};
unsigned long required_flags = SHRES_FLAG_FONT | SHRES_FLAG_16x16 | SHRES_FLAG_UNICODE;

char screen_setup_fonts(void)
{
  char err=0;
  unsigned char i;
  // Open the fonts    

  for(i=0;i<NUM_FONTS;i++) {

    try_or_fail(shopen(font_files[i],7,&shared.fonts[i]));
    if (tof_r) {
      //      printf("ERROR: Failed to open font '%s'\n", font_files[i]);
      err++;
    }

  }
  

  return err;
}

extern uint8_t wait_sprite[21*8];

void screen_setup(void)
{
  // Blue background, black border
  POKE(0xD020,0);
  POKE(0xD021,6);

  // H640 + fast CPU + VIC-III extended attributes + Interlace
  POKE(0xD031,0xE1);
  
  // 16-bit text mode, alpha compositor, 40MHz, Sprite H640
  POKE(0xD054,0xD5);

  // BOLD = ALT PALETTE so that REVERSE works for FCM
  POKE(0xD053,PEEK(0xD053)|0x10);

  // Disable RRB wrap-around
  POKE(0xD07C,PEEK(0xD07C)|0x08);
  
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

  // Turn off sprites initially
  POKE(0xD015,0);

  // But setup SMS and contact list scroll bar sprites
  POKE(0xD055,3); // Extended sprite height
  POKE(0xD056,SCROLL_BAR_HEIGHT);  // Extended height sprites for scroll bars
  POKE(0xD010,1); // SMS thread scroll bar goes on the right

  // Set the sprite pointer somewhere convenient @ $F000
  POKE(0xD06C,0x00);
  POKE(0xD06D,0xF0);
  POKE(0xD06E,0x80); // Use 16-bit sprite numbers

  // Point the sprite data for scroll bar sprites at $F100 & $F400
  // to allow for 255x3 = ~$300 bytes
  POKE(0xF000,((0xF100L>>6)) & 0xff);
  POKE(0xF001,(0xF100L>>(6+8)));
  POKE(0xF002,((0xF400L>>6)) & 0xff);
  POKE(0xF003,(0xF400L>>(6+8)));
  // Point the sprite data for the "wait" sprite at $F700
  POKE(0xF004,((0xF700L>>6)) & 0xff);
  POKE(0xF005,(0xF700L>>(6+8)));
  // And copy sprite data into place
  lcopy((unsigned long)&wait_sprite[0],0xF700,21*8);

  // Place "wait" sprite centrally on the screen
  POKE(0xD004,0x80);
  POKE(0xD005,0x80);

  // "Wait" sprite is black
  POKE(0xD029,0x00);
  // "Wait" sprite is 64px wide
  POKE(0xD057,0x04);
  
  // Position scroll-bar sprites appropriately
  POKE(0xD000L,0xCB);
  POKE(0xD001L,SCROLL_BAR_Y);
  POKE(0xD002L,0x80);
  POKE(0xD003L,0x1E);
  POKE(0xD010L,0x04); // Sprite X MSB only for "wait" sprite
  POKE(0xD05FL,0x01); // Sprite 0 X position super-MSB

  // Scroll-bar Sprites are MCM
  POKE(0xD01C,0x03);
  
  // Scroll-bar Sprites are medium grey
  POKE(0xD027,0x0C);
  POKE(0xD028,0x0C);

  // Sprite multi-colour 1 = black, so that we can draw scroll bars
  POKE(0xD025,0x0B); // Sprite MCM 0 = dark grey 
  POKE(0xD026,0x06); // Sprite MCM 1 = blue

  reset_glyph_cache();

  // Load dialpad and related glyphs
  mega65_chdir("PHONE");
  read_file_from_sdcard("dialpad.NCM",0x10000L);  
  
}

void show_busy(void)
{
  // Reset weight position when showing it
  if (!(PEEK(0xD015)&0x04)) {
    // Reset weight to near top of the screen
    POKE(0xD005,0x18);
    // Randomise X position somewhat
    POKE(0xD004,PEEK(0xD012));
  }
  
  POKE(0xD015,PEEK(0xD015)|0x04);
}

void hide_busy(void)
{
  POKE(0xD015,PEEK(0xD015)&(0xff-0x04));
}

char draw_scrollbar(unsigned char sprite_num,
		    unsigned int start,
		    unsigned int end,
		    unsigned int total)
{
  unsigned char first;
  unsigned char last;
  unsigned long temp;

  // Enable sprite for scroll bar
  POKE(0xD015,PEEK(0xD015)|(1<<sprite_num));
  
  if (!total) total=1;
  if (start>total) start=0;
  if (end>total) end=total;

  if (!start) first=0;
  else {  
    temp = start*SCROLL_BAR_HEIGHT;
    temp /= total;
    first = temp;
  }

  if (end == total) last=SCROLL_BAR_HEIGHT;
  else {
    temp = end*SCROLL_BAR_HEIGHT;
    temp /= total;
    last = temp;
  }

  lfill(0xf100 + (sprite_num*0x300), 0x55, 0x300);
  lfill(0xf100 + (sprite_num*0x300) + first*3, 0xAA, (last-first+1)*3);
  
  return 0;  
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

void screen_clear_partial_line(unsigned char row,
			       unsigned char first_col,
			       unsigned char last_col)
{
  unsigned int offset = row*(256*2) + first_col*2;
  unsigned int count = (last_col-first_col+1)*2;

  // Clear screen RAM using overlapping DMA  
  lpoke(screen_ram + offset + 0,0x20);
  lpoke(screen_ram + offset + 1,0x00);
  if (last_col>first_col) {
    lpoke(screen_ram + offset + 2,0x20);
    lpoke(screen_ram + offset + 3,0x00);
  }
  if (count>4) lcopy(screen_ram + offset, screen_ram + offset + 4,count - 4);

  // Clear colour RAM
  lfill(colour_ram + offset,0x01,count);
}

void screen_clear(void)
{
  // Clear screen RAM using overlapping DMA  
  lpoke(screen_ram + 0,0x20);
  lpoke(screen_ram + 1,0x00);
  lpoke(screen_ram + 2,0x20);
  lpoke(screen_ram + 3,0x00);
  lcopy(screen_ram, screen_ram + 4,(256*31*2) - 4);

  // Clear colour RAM
  lfill(colour_ram,0x01,(256*31*2));

}

extern unsigned char screen_ram_1_left[64];
extern unsigned char screen_ram_1_right[64];
extern unsigned char colour_ram_0_left[64];
extern unsigned char colour_ram_0_right[64];
extern unsigned char colour_ram_1[64];

unsigned int row0_offset;

#define BYTES_PER_GLYPH 256
unsigned char glyph_buffer[BYTES_PER_GLYPH];

void draw_goto(int x,int y, uint16_t goto_pos)
{
  // Get offset within screen and colour RAM for both rows of chars
  row0_offset = (y<<9) + (x<<1);

  lpoke(screen_ram + row0_offset + 0, goto_pos);
  lpoke(screen_ram + row0_offset + 1, 0x00 + ((goto_pos>>8)&0x3));
  lpoke(colour_ram + row0_offset + 0, 0x10);
  lpoke(colour_ram + row0_offset + 1, 0x00);
  
}

void reset_glyph_cache(void)
{
  unsigned long ofs;
  for(ofs=0;ofs<((uint32_t)GLYPH_CACHE_SIZE * BYTES_PER_GLYPH);) {
    unsigned long count = ((uint32_t)GLYPH_CACHE_SIZE * BYTES_PER_GLYPH) - ofs;
    if (count>65536) count=65536;
    lfill(GLYPH_DATA_START,0x00,(size_t)count);
    ofs+=count;
  }
  lfill((unsigned long)shared.cached_codepoints,0x00,GLYPH_CACHE_SIZE*sizeof(unsigned long));

  // Allocate unicode point 0x00001 = pseudo cursor
  // (2px wide full height bar)
  shared.cached_codepoints[0]=0x1;
  shared.cached_fontnums[0]=FONT_ALL;
  shared.cached_glyph_flags[0]=0x03;
#ifdef GRADED_CURSOR_ENDS
  lfill(GLYPH_DATA_START+0x00,0x80,0x100);
  lfill(GLYPH_DATA_START+0x08,0xFF,0x38);
  lfill(GLYPH_DATA_START+0x40,0xFF,0x38);
#else
  lfill(GLYPH_DATA_START+0x00,0xD0,0x100);
#endif
  
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
  
  if (shseek(&shared.fonts[font],codepoint<<8,SEEK_SET)) {
    // Glyph doesn't exist in this font.
    return;
  }

  try_or_fail(shread(glyph_buffer,256,&shared.fonts[font]));
  
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
  lcopy((unsigned long)glyph_buffer,GLYPH_DATA_START + ((unsigned long)cache_slot<<8), BYTES_PER_GLYPH);
  shared.cached_codepoints[cache_slot]=codepoint;
  shared.cached_fontnums[cache_slot]=font;
  shared.cached_glyph_flags[cache_slot]=glyph_flags;

}

char pad_string_viewport(unsigned char x_glyph_start, unsigned char y_glyph, // Starting coordinates in glyphs
			 unsigned char colour,
			 unsigned int x_pixels_viewport_width,
			 unsigned char x_glyphs_viewport,
			 unsigned int x_viewport_absolute_end_pixel){

  if (x_glyph_start > x_glyphs_viewport) {
    // x_glyphs_viewport is the right-hand glyph of the viewport, not it's width

    // As we are only padding, we'll treat this as a soft error, rather than calling
    // fail().
    // fail(1);
    
    return 1;
  }
  
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

  while(x<x_pixels_viewport_width) {
    // How many pixels for this glyph
    px=16;
    if (px>(x_pixels_viewport_width-x)) px=x_pixels_viewport_width-x;

    trim = 16 - px;
    
    // Ran out of glyphs to make the alignment
    if (x_g==x_glyphs_viewport) {
      return 1;
    }

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
  unsigned char ff;

  if (x_glyph_start > x_glyphs_viewport) {
    // x_glyphs_viewport is the right-hand glyph of the viewport, not it's width
    fail(1);
  }
    
  if (pixels_used) *pixels_used = 0;

  // Allow drawing of string segments
  if (str_end) {
    if (utf8>=str_end) return 0;
  }

  while ( (cp = utf8_next_codepoint(&utf8)) != 0L ) {

    // Fall-back to emoji font when required if using the UI font
    ff = pick_font_by_codepoint(cp,f);
    
    // Abort if the glyph won't fit.
    if (lookup_glyph(ff,cp,&glyph_pixels, NULL) + x >= x_glyphs_viewport) break;
    if (glyph_pixels + pixels_wide > x_pixels_viewport) break;

    // Glyph fits, so draw it, and update our dimension trackers
    glyph_pixels = 0;
    x += draw_glyph(x_glyph_start + x, y_glyph_start, ff, cp, colour, &glyph_pixels);
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

  switch(padP) {
  case VIEWPORT_PADDED_LEFT:
    // Copy the rendered glyphs up
    screen_shuffle_glyphs_right(x_glyph_start, y_glyph_start,
				x,
				x_glyphs_viewport - x);
    
    pad_string_viewport(x_glyph_start, y_glyph_start, // Starting coordinates in glyphs
			colour,
		        x_pixels_viewport - pixels_wide,  // Pixels remaining in viewport
			x_glyphs_viewport - x, // Right hand glyph of view port
			x_start_px + (x_pixels_viewport - pixels_wide)); // VIC-IV pixel column to point GOTOX to
    break;
  case VIEWPORT_PADDED_RIGHT:
    pad_string_viewport(x+ x_glyph_start, y_glyph_start, // Starting coordinates in glyphs
			colour,
		        x_pixels_viewport - pixels_wide,  // Pixels remaining in viewport
			x_glyphs_viewport, // Right hand glyph of view port
			x_start_px + x_pixels_viewport); // VIC-IV pixel column to point GOTOX to
    break;
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
				 unsigned long next_cp)
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
  unsigned int ofs;
  unsigned int best_break_ofs;
  unsigned int best_break_cost;
  unsigned char *best_break_s;
  unsigned char *last_break_s;
  unsigned char break_required;
  unsigned int this_cost, underful_cost;

  buffers.textbox.line_count = 1;
  
  // Box must be wide enough to take single widest glyph
  if (box_width_pixels<32) { fail(6); return 6; }
  if (box_width_glyphs<2) { fail(5); return 5; }
  
  buffers_lock(LOCK_TEXTBOX);

  if (!str) { fail(4); return 4; }
  if (!*str) {
    // For an empty string, just reserve a single line, so that
    // it's still visible.
    buffers.textbox.line_offsets_in_bytes[0]=0;
    return 0;
  }

  r = string_render_analyse(str, font,
			    &buffers.textbox.len,
			    buffers.textbox.pixel_widths,
			    buffers.textbox.glyph_widths,
			    buffers.textbox.break_costs);

  w_g=0;
  w_px=0;
  ofs=0;
  s = str;

  best_break_ofs=0;
  best_break_cost=0xfff;
  best_break_s=s;
  last_break_s=s;

  if (r) return r;

  buffers.textbox.line_count=0;

  // Routine requires null-terminated string
  if (buffers.textbox.draft_len>=RECORD_DATA_SIZE)
    buffers.textbox.draft[RECORD_DATA_SIZE-1]=0;
  else
    buffers.textbox.draft[buffers.textbox.draft_len]=0;  

  while(*s) {

    // Advance through the string, one unicode code point at a time.
    // We don't need to know the code-point, because we have already done the code-point
    // sensitive analysis in string_render_analyse().
    utf8_next_codepoint(&s);

    break_required=0;
    // Run out of space yet? (we keep one glyph spare for a final GOTOX to ensure alignment)
    if (w_g + buffers.textbox.glyph_widths[ofs] > (box_width_glyphs -1)) break_required=1;
    if (w_px + buffers.textbox.pixel_widths[ofs] > box_width_pixels) break_required=1;

    if (break_required) {
      // If a line break is required, record it, and look for next line.
      buffers.textbox.line_offsets_in_bytes[buffers.textbox.line_count++]=(best_break_s-last_break_s);

      if (!best_break_ofs) {
	fail(7);
	return 7;
      }

      w_px=0;
      w_g=0;
      ofs=best_break_ofs;
      s=best_break_s;
      best_break_ofs=0;
      best_break_cost=0xfff;
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
			 padding,
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
  unsigned char ff;

  if (!str) return 1;
  
  /* To compute break-after cost, we sometimes want to peek the next codepoint. */
  while (*s && o < RECORD_DATA_SIZE) {
    unsigned char pixel_count = 0;
    unsigned char glyph_count = 0;

    /* Decode current cp */
    unsigned long cp = utf8_next_codepoint(&s);

    /* Peek the next codepoint (without consuming original stream). */
    unsigned char *peek = s;
    unsigned long next_cp = *peek ? utf8_next_codepoint(&peek) : 0;

    /* Measure glyph/pixel widths for this cp */
    ff = pick_font_by_codepoint(cp,font);
    
    glyph_count = lookup_glyph(ff, cp, &pixel_count, NULL);
    
    if (pixel_widths) pixel_widths[o] = pixel_count;
    if (glyph_widths) glyph_widths[o] = glyph_count;

    /* Compute break cost AFTER this cp */
    if (break_costs) {
      /* If this is the very first character, never prefer a break (useless). */
      unsigned char cost = compute_break_cost(cp, next_cp);

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
    if (!shared.cached_codepoints[i]) break;
    if (shared.cached_codepoints[i]==codepoint
	&&((shared.cached_fontnums[i]==FONT_ALL)||(shared.cached_fontnums[i]==font))) break;
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

  if (shared.cached_codepoints[i]!=codepoint) {
    load_glyph(font, codepoint, i);
  } 

  if (pixels_used) *pixels_used = shared.cached_glyph_flags[i]&0x1f;

  if (glyph_id) *glyph_id = i;

  // How many glyphs does it use?
  if (shared.cached_glyph_flags[i]>8) return 2; else return 1;
}

char draw_glyph(int x, int y, int font, unsigned long codepoint,unsigned char colour, unsigned char *pixels_used)
{
  unsigned int i = 0x7fff;
  unsigned char table_index; 
  unsigned char reverse = colour & 0x80; // MSB of colour indicates reverse

  // Make reverse bit be in correct place for SEAM colour RAM byte 1 reverse flag
  if (reverse) reverse = 0x20;

  // Make cursor blink
  if (codepoint==CURSOR_CHAR) reverse |= 0x10;
  
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
  table_index = shared.cached_glyph_flags[i]&0x1f;
  table_index |= (shared.cached_glyph_flags[i]>>2)&0x20;

  // Get offset within screen and colour RAM for both rows of chars
  row0_offset = (y<<9) + (x<<1);

  // Set screen RAM
  lpoke(screen_ram + row0_offset + 0, ((i&0x3f)<<2) + 0 );
  lpoke(screen_ram + row0_offset + 1, screen_ram_1_left[table_index] + (i>>6) + 0x10);

  // Set colour RAM
  lpoke(colour_ram + row0_offset + 0, colour_ram_0_left[table_index]);
  lpoke(colour_ram + row0_offset + 1, ((colour_ram_1[table_index]+colour+reverse) ));

  // Set the number of pixels wide
  if (pixels_used) *pixels_used = shared.cached_glyph_flags[i]&0x1f;
  
  // And the 2nd column, if required
  if ((shared.cached_glyph_flags[i]&0x1f)>8) {
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

uint8_t shuffle_buffer[256];
char screen_shuffle_glyphs_right(uint8_t x_source, uint8_t y,
				 uint8_t width_gl,
				 uint8_t x_dest)
{
  uint16_t src_offset = (y<<9) + (x_source<<1);
  uint16_t dst_offset = (y<<9) + (x_dest<<1);
  
  if (width_gl>127) return 1;

  if (!width_gl) return 0;
  
  lcopy((unsigned long)screen_ram + src_offset,
	(unsigned long)shuffle_buffer, width_gl*2);
  lcopy((unsigned long)shuffle_buffer,
	(unsigned long)screen_ram + dst_offset, width_gl*2);
  
  lcopy((unsigned long)colour_ram + src_offset,
	(unsigned long)shuffle_buffer, width_gl*2);
  lcopy((unsigned long)shuffle_buffer,
	(unsigned long)colour_ram + dst_offset, width_gl*2);  

  return 0;
}


unsigned long utf8_next_codepoint(unsigned char **s)
{
  unsigned char *p;
  unsigned long cp = 0;

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
    cp = ((unsigned long)(p[0] & 0x0F) << 12) |
      ((unsigned long)(p[1] & 0x3F) << 6) |
         (p[2] & 0x3F);
    *s += 3;
    return cp;
  }

  // 4-byte sequence: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
  // e.g., f0 9f 8d 92
  if ((p[0] & 0xF8) == 0xF0) {
    if ((p[1] & 0xC0) != 0x80 || (p[2] & 0xC0) != 0x80 || (p[3] & 0xC0) != 0x80)
      return 0xFFFDL;
    cp = (((unsigned long)(p[0] & 0x07)) << 18) |
      (((unsigned long)(p[1] & 0x3F)) << 12) |
       (((unsigned long)(p[2] & 0x3F)) << 6) |
      (p[3] & 0x3F);
    *s += 4;
    return cp;
  }

  // Invalid or unsupported UTF-8 byte
  (*s)++;
  return 0xFFFDL;
}

char pick_font_by_codepoint(unsigned long cp, unsigned char default_font)
{
    // Common emoji ranges
    if ((cp >= 0x1F300L && cp <= 0x1F5FFL) ||  // Misc Symbols and Pictographs
        (cp >= 0x1F600L && cp <= 0x1F64FL) ||  // Emoticons
        (cp >= 0x1F680L && cp <= 0x1F6FFL) ||  // Transport & Map Symbols
        (cp >= 0x1F700L && cp <= 0x1F77FL) ||  // Alchemical Symbols
        (cp >= 0x1F780L && cp <= 0x1F7FFL) ||  // Geometric Extended
        (cp >= 0x1F800L && cp <= 0x1F8FFL) ||  // Supplemental Arrows-C (used for emoji components)
        (cp >= 0x1F900L && cp <= 0x1F9FFL) ||  // Supplemental Symbols and Pictographs
        (cp >= 0x1FA00L && cp <= 0x1FA6FL) ||  // Symbols and Pictographs Extended-A
        (cp >= 0x1FA70L && cp <= 0x1FAFFL) ||  // Symbols and Pictographs Extended-B
        (cp >= 0x2600 && cp <= 0x26FF)   ||  // Misc symbols (some emoji-like)
        (cp >= 0x2700 && cp <= 0x27BF)   ||  // Dingbats
        (cp >= 0xFE00 && cp <= 0xFE0F)   ||  // Variation Selectors (used with emoji)
        (cp >= 0x1F1E6L && cp <= 0x1F1FFL))    // Regional Indicator Symbols (🇦 – 🇿)
        return FONT_EMOJI_COLOUR;    
    
    return default_font;
}

void textbox_insert_cursor(uint16_t ofs)
{
  if (ofs > buffers.textbox.draft_len) { fail(1); return; }
  if (buffers.textbox.draft_len >= (RECORD_DATA_SIZE-1)) { fail(2); return; }

  buffers.textbox.draft_cursor_position=ofs;
  if ( (ofs<buffers.textbox.draft_len)
      && (buffers.textbox.draft_len != buffers.textbox.draft_cursor_position)) {
    lcopy((unsigned long)&buffers.textbox.draft[ofs],
	  (unsigned long)&buffers.textbox.draft[ofs+1],
	  buffers.textbox.draft_len - buffers.textbox.draft_cursor_position);
  }
  buffers.textbox.draft[ofs]=CURSOR_CHAR;
  buffers.textbox.draft_len++;
	  
}

void textbox_find_cursor(void)
{
  buffers.textbox.draft_len = strlen((char *)buffers.textbox.draft);
  buffers.textbox.draft_cursor_position = buffers.textbox.draft_len;
  // Reposition cursor to first CURSOR_CHAR in the draft
  // (and remove any later ones)
  for(buffers.textbox.draft_cursor_position = 0;
      buffers.textbox.draft_cursor_position<buffers.textbox.draft_len;
      buffers.textbox.draft_cursor_position++) {
    if (buffers.textbox.draft[buffers.textbox.draft_cursor_position]==CURSOR_CHAR) {
      for(uint16_t position = buffers.textbox.draft_cursor_position + 1;
	  position<buffers.textbox.draft_len;
	  position++) {
	if (buffers.textbox.draft[position]==CURSOR_CHAR) {
	  // Found an extra cursor: Delete it.
	  if (buffers.textbox.draft_len != position)
	    lcopy((unsigned long)&buffers.textbox.draft[position+1],
		  (unsigned long)&buffers.textbox.draft[position],
		  buffers.textbox.draft_len - position);
	  buffers.textbox.draft_len--;
	}
      }
      // Finished removing extra cursor characters
      break;	    
    }
  }
  // No cursor found, so append one to the end (trimming the draft if necessary to make it fit)
  if (buffers.textbox.draft_cursor_position >= buffers.textbox.draft_len) {
    if (buffers.textbox.draft_len > (RECORD_DATA_SIZE - 2))
      buffers.textbox.draft_len = (RECORD_DATA_SIZE - 2);
    buffers.textbox.draft[buffers.textbox.draft_len++]=CURSOR_CHAR;
    buffers.textbox.draft[buffers.textbox.draft_len]=0;
  }
}

void textbox_remove_cursor(void)
{
  if (buffers.textbox.draft_len==0) return;

  if (buffers.textbox.draft_len >= RECORD_DATA_SIZE) {
    fail(1);
    return;
  }

  if (!buffers.textbox.draft_len) return;

  // Don't remove if the cursor is already gone
  if (buffers.textbox.draft[buffers.textbox.draft_cursor_position]!=CURSOR_CHAR) return;
    
  if (buffers.textbox.draft_cursor_position>=buffers.textbox.draft_len) {
    // Cursor has already been removed
    return;
  }

  if (buffers.textbox.draft_len != buffers.textbox.draft_cursor_position)
    lcopy((unsigned long)&buffers.textbox.draft[buffers.textbox.draft_cursor_position+1],
	  (unsigned long)&buffers.textbox.draft[buffers.textbox.draft_cursor_position],
	  buffers.textbox.draft_len - buffers.textbox.draft_cursor_position);
  buffers.textbox.draft_len--;
}

unsigned char nybl_to_char(unsigned char n)
{
  if (n<0xa) return n+'0';
  return (n-0xa)+'A';
}

void bcd_to_str(unsigned char v, unsigned char *out)
{
  out[0]=nybl_to_char(v>>4);
  out[1]=nybl_to_char(v&0xf);
  return;
}
