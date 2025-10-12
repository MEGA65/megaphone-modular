#define RENDER_COLUMNS 145
#define MAX_ROWS 30

// Space for dummy line, status bar, a gap, a contact and a gap
#define SMS_FIRST_ROW (1 + 1 + 1 + 3 + 1)

#define RIGHT_AREA_START_PX 360
#define SMS_TX_RX_OFFSET_PX 24
#define SMS_TX_RX_OFFSET_GL (SMS_TX_RX_OFFSET_PX/8)
#define RIGHT_AREA_START_GL 45
#define RIGHT_AREA_WIDTH_PX 294

#define SMS_TEXT_BLOCK_WIDTH (RIGHT_AREA_WIDTH_PX - 32)

#define SCROLL_BAR_HEIGHT (23*8)
#define SCROLL_BAR_Y (0x1e + 6*8)

#define NUM_FONTS 4
#define FONT_EMOJI_COLOUR 0
#define FONT_EMOJI_MONO 1
#define FONT_TEXT 2
#define FONT_UI 3
// Allow a glyph to be forced to be used no matter which font is requested
#define FONT_ALL 0xff

// Magic unicode codepoint that instead draws a blinking 2px wide cursor
#define CURSOR_CHAR 0x01

void show_busy(void);
void hide_busy(void);

void screen_setup(void);
char screen_setup_fonts(void);
void generate_rgb332_palette(void);
void screen_clear(void);
void screen_clear_partial_line(unsigned char row,
			       unsigned char first_col,
			       unsigned char last_col);
void draw_goto(int x,int y, uint16_t goto_pos);
char draw_glyph(int x, int y, int font, unsigned long codepoint,unsigned char colour, unsigned char *pixels_used);
char screen_shuffle_glyphs_right(uint8_t x_source, uint8_t y,
				 uint8_t width_gl,
				 uint8_t x_dest);
#define VIEWPORT_PADDED_LEFT 2
#define VIEWPORT_PADDED_RIGHT 1
#define VIEWPORT_PADDED 1
#define VIEWPORT_UNPADDED 0
char draw_string_nowrap(unsigned char x_glyph_start, unsigned char y_glyph_start, // Starting coordinates in glyphs
			unsigned char f, // font
			unsigned char colour, // colour
			unsigned char *utf8,		     // Number of pixels available for width
			unsigned int x_pixel_start,
			unsigned int x_pixels_viewport,
			// Number of glyphs available
			unsigned char x_glyphs_viewport,
		        unsigned char *str_end,
			unsigned char padP,
			// And return the number of each consumed
			unsigned int *pixels_used,
			unsigned char *glyphs_used);

char textbox_draw(unsigned char x_start,
		  unsigned char y_start,
		  unsigned int x_pixel_start,
		  unsigned int box_width_pixels,
		  unsigned int box_width_glyphs,
		  unsigned char font,
		  unsigned char colour,
		  unsigned char *str,
		  unsigned int first_row,
		  unsigned int last_row,
		  unsigned char padding);

void reset_glyph_cache(void);
void load_glyph(int font, unsigned long codepoint, unsigned int cache_slot);
unsigned char lookup_glyph(int font, unsigned long codepoint,unsigned char *pixels_used, unsigned int *glyph_id);

char pick_font_by_codepoint(unsigned long cp, unsigned char default_font);
unsigned long utf8_next_codepoint(unsigned char **s);

char calc_break_points(unsigned char *str,int font,unsigned int box_width_pixels, unsigned int box_width_glyphs);
char string_render_analyse(unsigned char *str,
                           int font,
                           unsigned int *len,
                           unsigned char *pixel_widths, /* [RECORD_DATA_SIZE] */
                           unsigned char *glyph_widths, /* [RECORD_DATA_SIZE] */
                           unsigned int *break_costs   /* [RECORD_DATA_SIZE] */
                           );

char draw_scrollbar(unsigned char sprite_num,
		    unsigned int start,
		    unsigned int end,
		    unsigned int total);

void textbox_find_cursor(void);
void textbox_remove_cursor(void);

unsigned char nybl_to_char(unsigned char n);
void bcd_to_str(unsigned char v, unsigned char *out);


extern unsigned long screen_ram;
extern unsigned long colour_ram;

