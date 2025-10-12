#include "includes.h"

#include "contactscreens.h"

#include "buffers.h"
#include "screen.h"
#include "records.h"
#include "af.h"

char contact_draw(uint8_t x, uint8_t y,
		  uint16_t x_start_px,
		  uint8_t w_gl, uint16_t w_px,
		  uint16_t contact_id,
		  uint8_t active_field)
{
  if (buffers_lock(LOCK_TEXTBOX)) fail(99);
  
  if (w_gl<20) return 1;
  if (w_px<96) return 2;

  unsigned char *labels[3]
    ={(unsigned char*)"First: ",(unsigned char *)"Last: ",(unsigned char *)"Phone: "};
    
  for(uint8_t field=0;field<3;field++) {
    
    draw_string_nowrap(x, y+field,
		       FONT_UI,
		       0x0f, // light grey for label text
		       labels[field],
		       x_start_px,
		       LABEL_WIDTH_PX,
		       x + LABEL_WIDTH_GL,
		       NULL,
		       VIEWPORT_PADDED_LEFT,
		       NULL,
		       NULL);

    af_retrieve(field + AF_CONTACT_FIRSTNAME, active_field, contact_id);
    af_redraw(active_field, field + AF_CONTACT_FIRSTNAME);
  }

  return 0;
}
