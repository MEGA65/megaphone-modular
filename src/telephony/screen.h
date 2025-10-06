#define RENDER_COLUMNS 160
#define MAX_ROWS 30

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

void draw_goto(int x,int y, int goto_pos);
void screen_setup(void);
char screen_setup_fonts(void);
void generate_rgb332_palette(void);
void screen_clear(void);
void screen_clear_partial_line(unsigned char row,
			       unsigned char first_col,
			       unsigned char last_col);
void draw_goto(int x,int y, int goto_pos);
char draw_glyph(int x, int y, int font, unsigned long codepoint,unsigned char colour, unsigned char *pixels_used);
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


extern unsigned long screen_ram;
extern unsigned long colour_ram;


