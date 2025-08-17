#define RENDER_COLUMNS 128
#define MAX_ROWS 30

#define NUM_FONTS 4
#define FONT_EMOJI_COLOUR 0
#define FONT_EMOJI_MONO 1
#define FONT_TEXT 2
#define FONT_UI 3

void draw_goto(int x,int y, int goto_pos);
void screen_setup(void);
char screen_setup_fonts(void);
void generate_rgb332_palette(void);
void screen_clear(void);
void draw_goto(int x,int y, int goto_pos);
char draw_glyph(int x, int y, int font, unsigned long codepoint,unsigned char colour, unsigned char *pixels_used);

void reset_glyph_cache(void);
void load_glyph(int font, unsigned long codepoint, unsigned int cache_slot);
unsigned char lookup_glyph(int font, unsigned long codepoint,unsigned char *pixels_used, unsigned int *glyph_id);

char pick_font_by_codepoint(unsigned long cp);
unsigned long utf8_next_codepoint(unsigned char **s);

extern unsigned long screen_ram;
extern unsigned long colour_ram;


