#include "includes.h"

#include "contactscreens.h"

#include "screen.h"
#include "records.h"

// label width must be odd to not cause weird GOTOX glitch with CHARY16 mode
#define LABEL_WIDTH_PX 47
#define LABEL_WIDTH_GL 10

char contact_draw(uint8_t x, uint8_t y,
		  uint16_t x_start_px,
		  uint8_t w_gl, uint16_t w_px,
		  unsigned int contact_id,
		  uint8_t active_field,
		  unsigned char *contact_record)
{
  unsigned char *string;

  if (w_gl<20) return 1;
  if (w_px<96) return 2;

  uint8_t fields[3]={FIELD_FIRSTNAME, FIELD_LASTNAME, FIELD_PHONENUMBER};
  unsigned char *labels[3]
    ={(unsigned char*)"First:",(unsigned char *)"Last:",(unsigned char *)"Phone:"};
    
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

    string = find_field(contact_record, RECORD_DATA_SIZE, fields[field],NULL);
    draw_string_nowrap(x + LABEL_WIDTH_GL, y+field,
		       FONT_UI,
		       active_field==(field+1) ? 0x8f : 0x8b, // reverse medium grey if not selected
		       (unsigned char *)string,
		       x_start_px + LABEL_WIDTH_PX,
		       w_px - LABEL_WIDTH_PX,
		       x + w_gl,
		       NULL,
		       VIEWPORT_PADDED_RIGHT,
		       NULL,
		       NULL);
  }

  return 0;
}
