#include "includes.h"

#include "contactscreens.h"

#include "buffers.h"
#include "screen.h"
#include "records.h"
#include "af.h"
#include "mountstate.h"

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

  af_retrieve(AF_CONTACT_FIRSTNAME, active_field, contact_id);
  
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
    af_retrieve_fast(field + AF_CONTACT_FIRSTNAME, active_field, contact_id);
    if (active_field == AF_ALL)
      af_redraw(field + AF_CONTACT_FIRSTNAME,
		field + AF_CONTACT_FIRSTNAME,y);
    else if (active_field == AF_NONE)
      af_redraw(99,field + AF_CONTACT_FIRSTNAME, y);
    else
      af_redraw(active_field, field + AF_CONTACT_FIRSTNAME, y);
  }

  return 0;
}

char contact_draw_list(int16_t last_contact, int16_t current_contact)
{
  // We use the same vertical space as for SMS thread, so that we don't have
  // to mess with the scroll bar dimensions.
  const int display_count = (MAX_ROWS + 1 - SMS_FIRST_ROW) / 4;

  // We don't care that the blank line below the contact is below the
  // scroll bar. This then lets us get an integer number of contacts
  // on the screen.
  int8_t next_row = MAX_ROWS + 1 - (3 + 1);

  // Erase the contact block at the top
  for(uint8_t l=0;l<4;l++)
    screen_clear_partial_line(1+1+1+l,RIGHT_AREA_START_GL, RENDER_COLUMNS);
  
  mount_state_set(MS_CONTACT_LIST, current_contact);

  // Read BAM, find first free sector (if we don't write it back, it doesn't actually
  // get allocated, thus saving the need for a separate get allocated count function).
  read_sector(0,1,0);
  uint16_t contact_count = record_allocate_next(SECTOR_BUFFER_ADDRESS);
  
  if (last_contact < 0) {
    last_contact = contact_count - 1;
  }

  if (last_contact < display_count) last_contact = display_count - 1;

  while(next_row >= (SMS_FIRST_ROW - 1)) {
    // Highlight if current contact
    uint8_t activeP = AF_NONE;
    if (last_contact == current_contact) activeP = AF_ALL;

    if (last_contact > 0) {
      contact_draw(RIGHT_AREA_START_GL, next_row,
		   RIGHT_AREA_START_PX,
		   MAX_ROWS - RIGHT_AREA_START_GL, RIGHT_AREA_WIDTH_PX,
		   last_contact,
		   activeP);
      screen_clear_partial_line(next_row+3,RIGHT_AREA_START_GL, RENDER_COLUMNS);
      
    } else {
      // If insufficient contacts, clear the screen region
      for(uint8_t l=0;l<4;l++)
	screen_clear_partial_line(next_row+l,RIGHT_AREA_START_GL, RENDER_COLUMNS);
    }

    next_row -= 4;
    last_contact -= 1;
  }

  draw_scrollbar(0, // SMS thread scroll bar (which we re-use here)
		 last_contact,
		 last_contact + display_count,
		 contact_count-1-1);  // subtract for BAM, and also for 0-oriented count
  
  
  return 0;
}
