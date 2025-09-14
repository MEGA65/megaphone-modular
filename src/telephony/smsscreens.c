#include "includes.h"

#include "ascii.h"

#include "includes.h"

#include "buffers.h"
#include "screen.h"
#include "records.h"
#include "contacts.h"

char sms_thread_display(unsigned int contact,
			unsigned int last_message,
			char with_edit_box_P
			)
{
  int y=28;
  unsigned int bottom_row_available = 30; // allow all the way to bottom of screen for now.
  unsigned int bottom_row;
  unsigned int message_count = 0; 
  unsigned char r;

  buffers_lock(LOCK_TEXTBOX);

  // XXX - Remember what we have mounted somehwere, so that we don't waste lots of time re-mounting
  // for every screen refresh
  // record = 0 BAM, 1 = unknown contact place-holder. 2 = first real contact
  r=mount_contact_qso(contact);
  if (r) {
    lpoke(0x12002L,r);
    while(1) POKE(0xD021,PEEK(0xD012));
  }

  // Read BAM, find first free sector (if we don't write it back, it doesn't actually
  // get allocated, thus saving the need for a separate get allocated count function).
  read_sector(0,1,0);
  message_count = record_allocate_next(SECTOR_BUFFER_ADDRESS);
  message_count--; 

  if (last_message < message_count) message_count = last_message;

  lcopy(&message_count,0x12000L,2);

  y = MAX_ROWS;
  bottom_row_available = MAX_ROWS;

  lpoke(0xFFF0L,message_count>>0);
  lpoke(0xFFF1L,message_count>>8);
  lpoke(0xFFF2L,0x42);
  
  while(y>=2&&message_count>0) {

    unsigned int first_row = 0;
    unsigned char we_sent_it = 0;
    
    // Read the message
    read_record_by_id(0,message_count,buffers.textbox.record);

    // Get message direction
    buffers.textbox.field = find_field(buffers.textbox.record,
				       RECORD_DATA_SIZE,
				       FIELD_MESSAGE_DIRECTION,
				       &buffers.textbox.field_len);  
    if (buffers.textbox.field&&buffers.textbox.field[0]) we_sent_it=1;      
    
    // Get the message body
    buffers.textbox.field = find_field(buffers.textbox.record,
				       RECORD_DATA_SIZE,
				       FIELD_BODYTEXT,
				       &buffers.textbox.field_len);  

    // And figure out how many lines it will take on the screen.
    calc_break_points(buffers.textbox.field,
		      FONT_UI,
		      255, // px width
		      60   // glyph width
		      );

    // Adjust y to the necessary starting row.    
    y = y - buffers.textbox.line_count;
    first_row = 0;
    while(y<2) {
      y++;
      first_row++;
    }

    bottom_row = buffers.textbox.line_count-1;
    while ((y+bottom_row) > bottom_row_available) bottom_row--;
    
    textbox_draw(we_sent_it? 384/8 : 360/8, // column on screen
		 y, // row on screen
		 we_sent_it? 384 : 360, // start pixel
		 255, // px width
		 RENDER_COLUMNS - 1 - (we_sent_it? 48 : 45),   // glyph width
		 FONT_UI,
		 we_sent_it ? 0x8D : 0x8F, // colour
		 buffers.textbox.field,
		 first_row, // Starting row of text box
		 buffers.textbox.line_count-1, // Ending row of text box
		 VIEWPORT_PADDED);

    // Leave blank line between messages
    y--;
    
    message_count--;
  }    

#if 0
  // record 0 = BAM, 1 = first actual message
  read_record_by_id(0,1,buffers.textbox.record);

  buffers.textbox.field = find_field(buffers.textbox.record,
				     RECORD_DATA_SIZE,
				     FIELD_BODYTEXT,
				     &buffers.textbox.field_len);  
  
  calc_break_points(buffers.textbox.field,
		    FONT_UI,
		    255, // px width
		    60   // glyph width
		    );

  textbox_draw(360/8, // column on screen
	       2, // row on screen
	       360, // start pixel
	       255, // px width
	       RENDER_COLUMNS - 1 - 45,   // glyph width
	       FONT_UI,
	       0x8F, // colour
	       buffers.textbox.field,
	       0, // Starting row of text box
	       buffers.textbox.line_count-1, // Ending row of text box
	       VIEWPORT_PADDED);

  textbox_draw(384/8, // 384/8, // column on screen
	       12, // row on screen
	       384, // start pixel
	       270, // px width
	       RENDER_COLUMNS - 1 - 48,   // glyph width
	       FONT_UI,
	       0x8D, // colour
	       buffers.textbox.field,
	       0, // Starting row of text box
	       buffers.textbox.line_count-1, // Ending row of text box
	       VIEWPORT_PADDED);
#endif  

  return 0;
}
