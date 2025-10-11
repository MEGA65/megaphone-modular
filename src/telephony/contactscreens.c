#include "includes.h"

#include "contactscreens.h"

#include "screen.h"

char contact_draw(uint8_t x, uint8_t y,
		  uint16_t x_start_px,
		  uint8_t w_gl, uint16_t w_px,
		  unsigned int contact_id,
		  uint8_t active_field,
		  unsigned char *contact_record)
{
  if (w_gl<20) return 1;
  if (w_px<96) return 2;

  draw_string_nowrap(x, y,
		     FONT_UI,
		     0x0f, // light grey for label text
		     (unsigned char *)"Name:",
		     x_start_px,
		     x_start_px + 48,
		     12,
		     NULL,
		     VIEWPORT_PADDED,
		     NULL,
		     NULL);
  draw_string_nowrap(x, y+2,
		     FONT_UI,
		     0x0f, // light grey for label text
		     (unsigned char *)"Number:",
		     x_start_px,
		     x_start_px + 48,
		     12,
		     NULL,
		     VIEWPORT_PADDED,
		     NULL,
		     NULL);
  
  return 0;
}
