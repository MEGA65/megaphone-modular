#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#define HEIGHT 31
#define OUTPUT_HEIGHT 32
#define OUTPUT_WIDTH 64
#define BASELINE (OUTPUT_HEIGHT - 5)
#define MAX_WIDTH 1024

// Set glyph width extension
double stretch_x = 1.5; 


char output[OUTPUT_HEIGHT][MAX_WIDTH + 1];

FILE *outfile=NULL;

void record_glyph(unsigned char data[2560])
{
#define GLYPH_SIZE (64*4*2*2)
  
  if (outfile) {
    fwrite(data,64*4*2*2,1,outfile);
  }
}


int dump_bytes(char *msg, unsigned char *bytes, int length)
{
  fprintf(stdout, "%s:\n", msg);
  for (int i = 0; i < length; i += 16) {
    fprintf(stdout, "%04X: ", i);
    for (int j = 0; j < 16; j++)
      if (i + j < length)
        fprintf(stdout, " %02X", bytes[i + j]);
    fprintf(stdout, "\n");
  }
  return 0;
}

void clear_output() {
    for (int y = 0; y < OUTPUT_HEIGHT; y++) {
        memset(output[y], ' ', MAX_WIDTH);
        output[y][MAX_WIDTH-1] = '\0';
    }
}

void draw_bitmap(FT_Bitmap *bitmap, int bitmap_top, int pen_x) {
    int y_offset = BASELINE - bitmap_top;

    for (int y = 0; y < OUTPUT_HEIGHT; y++) {
        for (int x = 0; x < bitmap->width; x++) {
            int bx = x;
            int by = y - y_offset;

            if (by < 0 || by >= bitmap->rows)
                continue;

            int byte_index = by * bitmap->pitch + (bx >> 3);
            int bit_index = 7 - (bx & 7);
            unsigned char byte = bitmap->buffer[byte_index];
            int pixel_on = (byte >> bit_index) & 1;

            int out_x = pen_x + x;
            if (out_x >= 0 && out_x < MAX_WIDTH) {
                output[y][out_x] = pixel_on ? '#' : output[y][out_x];
            }
        }
    }
}

char intensity_to_ascii(uint8_t intensity) {
    // Density ramp from light to dark
    const char ramp[] = " .:-=+*#%@";
    const int ramp_len = sizeof(ramp) - 1; // exclude null terminator

    int index = (intensity * ramp_len) / 16;
    if (index >= ramp_len) index = ramp_len - 1;

    return ramp[index];
}

static void
pack_into_tiles(const uint8_t pixel_data[OUTPUT_HEIGHT][32], uint8_t data[2560])
{
    memset(data, 0, 2560);          /*  safety / predictable padding           */

    for (int y = 0; y < OUTPUT_HEIGHT; ++y) {          /* 0‥31 */
      for (int bx = 0; bx < 32; ++bx) {              /* byte-column 0‥31 = 64px     */

	const int intra_tile_row     = (y >> 1) & 7;               /* 0‥7   */
	const int interlace_even     = !(y & 1);             /* true if 0,2,4…       */
	const int interlace_offset   = interlace_even ?   0 : 64; /* tiles 0+1 or 2+3     */

	const int lower_row_offset = (y>15) ? (64*2*4) : 0;
	
	char char_column = bx / 8;

	const int tile_offset = interlace_offset + char_column * 128 + lower_row_offset;

	const int dest_index  = tile_offset + intra_tile_row * 8 + (bx & 7);

	//	fprintf(stderr,"DEBUG: y=%d, x=%d, dest_index=0x%04x\n",y,bx,dest_index);
	
	data[dest_index] = pixel_data[y][bx];
      }
    }
}

unsigned char nybl_pick(unsigned char c, int pickHigh)
{
  if (pickHigh) return c>>4;
  return c&0xf;
}

void render_glyph(FT_Face face, uint32_t codepoint, int pen_x) {
    clear_output();

    FT_Matrix matrix = {
        (FT_Fixed)(stretch_x * 0x10000),  // scale X
        0,
        0,
        (FT_Fixed)(1.0 * 0x10000)         // scale Y
    };
    FT_Set_Transform(face, &matrix, NULL);
    
    FT_UInt glyph_index = FT_Get_Char_Index(face, codepoint);
    if (glyph_index == 0) {
        fprintf(stderr, "Warning: Glyph not found for U+%04X\n", codepoint);
        return;
    }

#if 0
    if (FT_Load_Glyph(face, glyph_index, FT_LOAD_RENDER | FT_LOAD_TARGET_MONO)) {
        fprintf(stderr, "Warning: Could not render U+%04X in mono\n", codepoint);
    } else  {
      draw_bitmap(&face->glyph->bitmap, face->glyph->bitmap_top, pen_x);
    }
#endif
    
    if (FT_Load_Glyph(face, glyph_index, FT_LOAD_RENDER)) {
        fprintf(stderr, "Warning: Could not render U+%04X\n", codepoint);
        return;
    }

    
    FT_Bitmap *bmp = &face->glyph->bitmap;
    int width = face->glyph->advance.x >> 6;
    int is_color = (bmp->pixel_mode == FT_PIXEL_MODE_BGRA);

    if (width > 63 ) {
      fprintf(stderr,"WARNING: U+%05x is %dpx wide. Will be truncated to 64px\n",
	      codepoint, width);
      width = 64;
    }
    
    uint8_t glyph_width = width;  // For kerning-aware spacing

    uint8_t centring_offset = (64 - width) / 2;
    
    // Allocate pixel buffer
    uint8_t pixel_data[OUTPUT_HEIGHT][32] = {{0}}; // max 64px wide using nybls = 32 bytes per row

    int y_offset = BASELINE - face->glyph->bitmap_top;

    fprintf(stdout,"DEBUG: pixel mode = ");
    switch(bmp->pixel_mode) {
    case FT_PIXEL_MODE_NONE: printf("FT_PIXEL_MODE_NONE"); break;
    case FT_PIXEL_MODE_MONO: printf("FT_PIXEL_MODE_MONO"); break;
    case FT_PIXEL_MODE_GRAY: printf("FT_PIXEL_MODE_GRAY"); break;
    case FT_PIXEL_MODE_GRAY2: printf("FT_PIXEL_MODE_GRAY2"); break;
    case FT_PIXEL_MODE_GRAY4: printf("FT_PIXEL_MODE_GRAY4"); break;
    case FT_PIXEL_MODE_LCD: printf("FT_PIXEL_MODE_LCD"); break;
    case FT_PIXEL_MODE_LCD_V: printf("FT_PIXEL_MODE_LCD_V"); break;
    case FT_PIXEL_MODE_BGRA: printf("FT_PIXEL_MODE_BGRA"); break;
    default: printf("<unknown>");
    }
    printf("\n");

    printf("bmp->width = %d, bmp->rows = %d\n", bmp->width, bmp->rows);
    
    for (int y = 0; y < OUTPUT_HEIGHT; y++) {
        for (int x = 0; x < bmp->width && x < 64; x++) {
            int by = y - y_offset;
            if (by < 0 || by >= bmp->rows) continue;

	    printf(".");
	    
            uint8_t value = 0;	    
            if (is_color && bmp->pixel_mode == FT_PIXEL_MODE_BGRA) {
                uint8_t *p = &bmp->buffer[(by * bmp->width + x) * 4];
                uint8_t a = p[3];
                if (a < 16) continue;  // transparent
                // 332 color encoding
                uint8_t r = p[2] >> 5;
                uint8_t g = p[1] >> 5;
                uint8_t b = p[0] >> 6;
                value = (r << 5) | (g << 2) | b;
                if (value == 0) value = 0x01;  // 0 reserved for transparent
            }
            else if (bmp->pixel_mode == FT_PIXEL_MODE_GRAY) {
                value = bmp->buffer[by * bmp->pitch + x];
		printf("%02x ",value);

		// 4-bit: pack 2 pixels per byte
		if (x % 2 == 1) {
		  pixel_data[y][x / 2] &= 0x0f;
		  pixel_data[y][x / 2] |= (value >> 4) << 4;
		} else {
		  pixel_data[y][x / 2] &= 0xf0;
		  pixel_data[y][x / 2] |= (value >> 4);
		}

            }
            else if (bmp->pixel_mode == FT_PIXEL_MODE_MONO) {
                int byte = bmp->buffer[by * bmp->pitch + (x >> 3)];
                int bit = (byte >> (7 - (x & 7))) & 1;
                value = bit ? 255 : 0;

		if (x % 2 == 1) {
		  pixel_data[y][x / 2] &= 0x0f;
		  pixel_data[y][x / 2] |= (value >> 4) << 4;
		} else {
		  pixel_data[y][x / 2] &= 0xf0;
		  pixel_data[y][x / 2] |= (value >> 4);
		}
            }
	    else {
	      fprintf(stderr,"WARNING: Unsupported FT_PIXEL_MODE = %d\n",bmp->pixel_mode);
	    }
        }
    }

    // Optional: Print debug info
    printf("U+%04X (%s, %dpx): width=%d\n", codepoint,
           is_color ? "color" : "intensity",
           width, glyph_width);

    pen_x = centring_offset;
    for (int y = 0; y < OUTPUT_HEIGHT; y++) {
      for(int x=0;x<OUTPUT_WIDTH;x++) {
	output[y][pen_x + x] = intensity_to_ascii(nybl_pick(pixel_data[y][x/2],x&1));
      }
      output[y][pen_x + OUTPUT_WIDTH] = '|';
      output[y][pen_x + OUTPUT_WIDTH + 1] = 0;
      puts(output[y]);
    }
    puts("");

    unsigned char data[2560];

    pack_into_tiles(pixel_data, data);
          
    dump_bytes("Packed glyph",data,128*4*2);

    record_glyph(data);
    
}


int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <font_file.otf> [+hex_codepoint or ascii] [...]\n", argv[0]);
        return 1;
    }

    const char* font_path = argv[1];

    char filename[8192];
    snprintf(filename,8192,"dialpad.NCM");
    outfile=fopen(filename,"wb");
    
    FT_Library ft;
    if (FT_Init_FreeType(&ft)) {
        fprintf(stderr, "Could not initialize FreeType\n");
        return 1;
    }

    FT_Face face;
    if (FT_New_Face(ft, font_path, 0, &face)) {
        fprintf(stderr, "Could not load font: %s\n", font_path);
        FT_Done_FreeType(ft);
        return 1;
    }

    FT_Set_Pixel_Sizes(face, 0, HEIGHT);

    clear_output();

    int pen_x = 0;

    if (argc==2) {
      FT_ULong charcode;
      FT_UInt glyph_index;
      int pen_x = 0;
      
      charcode = FT_Get_First_Char(face, &glyph_index);
      while (glyph_index != 0) {
	render_glyph(face, charcode, pen_x);
	// Optionally: pen_x += face->glyph->advance.x >> 6; (not used in per-glyph rendering)
	charcode = FT_Get_Next_Char(face, charcode, &glyph_index);
      }      
    } else {
      for (int i = 2; i < argc; i++) {
        const char* arg = argv[i];
        size_t len = strlen(arg);
        for (size_t j = 0; j < len; j++) {
	  uint32_t codepoint;
	  if (j == 0 && arg[0] == '+') {
	    codepoint = (uint32_t)strtol(arg + 1, NULL, 16);
	    j = len;  // skip rest of string
	  } else {
	    codepoint = (uint8_t)arg[j];
	  }
	  
	  
	  render_glyph(face, codepoint,pen_x);
	  
	  // pen_x += face->glyph->advance.x >> 6;
        }
      }
      
    }

    FT_Done_Face(face);
    FT_Done_FreeType(ft);
    return 0;
}
