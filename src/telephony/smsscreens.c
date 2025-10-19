#include "includes.h"

#include "ascii.h"

#include "includes.h"

#include "buffers.h"
#include "screen.h"
#include "records.h"
#include "contacts.h"
#include "af.h"

void sms_thread_clear_screen_region(unsigned char first_row, unsigned char last_row)
{
  while(first_row<=last_row) {
    screen_clear_partial_line(first_row, 360/8, 255);
    first_row++;
  }
}

void textbox_erase_draft(void)
{
  buffers_lock(LOCK_TEXTBOX);
  buffers.textbox.draft_len = 1;
  buffers.textbox.draft_cursor_position = 0;
  lfill((uint32_t)buffers.textbox.draft,0x00,sizeof(buffers.textbox.draft));
  buffers.textbox.draft[0]=CURSOR_CHAR;
}

void textbox_hide_cursor(void)
{
  buffers_lock(LOCK_TEXTBOX);
  for(unsigned int i=0;i<buffers.textbox.draft_len;i++) {
    if (buffers.textbox.draft[i]==CURSOR_CHAR) {
      lcopy((unsigned long) &buffers.textbox.draft[i+1],
	    (unsigned long) &buffers.textbox.draft[i],
	    buffers.textbox.draft_len - i);
      if (buffers.textbox.draft_len>0) buffers.textbox.draft_len--;
      i--;
    }
  }
  buffers.textbox.draft[buffers.textbox.draft_len]=0;
  return;
}

char sms_thread_display(unsigned int contact,
			int last_message,
			char with_edit_box_P,
			char active_field,
			unsigned int *first_message_displayed
			)
{
  int y=28;
  unsigned int bottom_row_available = 30; // allow all the way to bottom of screen for now.
  unsigned int bottom_row;
  int message_count = 0;
  unsigned int message = 0;
  unsigned char r;

  // Draw GOTOX to right of dialpad, so that right display area
  // remains aligned.
  for(int y=2;y<MAX_ROWS;y++) draw_goto(RENDER_COLUMNS - 1,y,720);
  for(int y=2;y<MAX_ROWS;y++) draw_goto(100,y,720);
  
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

  // Allow negative positions to mean position from end of message stream
  if (last_message<0) last_message = message_count + (1+last_message);
  
  if (last_message < message_count) message = last_message;
  else message = message_count;

  lcopy((uint32_t)&message_count,0x12000L,2);
  
  if (with_edit_box_P) {  
    // Work out how many rows the message draft uses

    af_retrieve(AF_SMS, active_field, contact);

    TV8(">>line_count",buffers.textbox.line_count);
    TNL();
    
    try_or_fail(calc_break_points(buffers.textbox.draft,
				  FONT_UI,
				  RIGHT_AREA_WIDTH_PX, // text field in px
				  RENDER_COLUMNS - 1 - 45));
    
    y = MAX_ROWS - buffers.textbox.line_count - 1;
    bottom_row_available = MAX_ROWS - buffers.textbox.line_count - 1;
    
    // XXX - Remember what's on the screen already, and use DMA to scroll it up
    // and down, so that we don't need to redraw it all.
    // But for now, we don't have that, so just clear the thread area on the screen
    sms_thread_clear_screen_region(SMS_FIRST_ROW,MAX_ROWS);

    af_retrieve(AF_SMS, active_field, contact);
    if (active_field==AF_SMS) textbox_find_cursor();
    af_redraw(active_field,AF_SMS,0);
    
  } else {
    // No message editing text box visible
    y = MAX_ROWS - 1;
    bottom_row_available = MAX_ROWS - 1;    
  }  

  char partial_message=0;
  
  while(y>=SMS_FIRST_ROW&&message>0) {

    unsigned int first_row = 0;
    unsigned char we_sent_it = 0;
    
    // Read the message
    read_record_by_id(0,message,buffers.textbox.record);

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
		      SMS_TEXT_BLOCK_WIDTH, // px width
		      60   // glyph width
		      );

    // Adjust y to the necessary starting row.    
    y = y - buffers.textbox.line_count;
    first_row = 0;
    while(y<SMS_FIRST_ROW) {
      y++;
      first_row++;
      partial_message=1;
    }

    bottom_row = buffers.textbox.line_count-1;
    while ((y+bottom_row) > bottom_row_available) bottom_row--;
    
    textbox_draw(we_sent_it? (RIGHT_AREA_START_PX+SMS_TX_RX_OFFSET_PX)/8 : RIGHT_AREA_START_PX/8, // column on screen
		 y, // row on screen
		 we_sent_it? (RIGHT_AREA_START_PX+SMS_TX_RX_OFFSET_PX) : RIGHT_AREA_START_PX, // start pixel
		 SMS_TEXT_BLOCK_WIDTH, // px width
		 RENDER_COLUMNS - 1 - (we_sent_it? (RIGHT_AREA_START_GL+SMS_TX_RX_OFFSET_GL) : RIGHT_AREA_START_GL),   // glyph width
		 FONT_UI,
		 we_sent_it ? 0x8D : 0x8F, // colour
		 buffers.textbox.field,
		 first_row, // Starting row of text box
		 buffers.textbox.line_count-1, // Ending row of text box
		 VIEWPORT_PADDED);

    // Leave blank line between messages
    y--;

    if (partial_message) break;
    
    message--;
  }    

  POKE(0xF080,message>>0);
  POKE(0xF081,message>>8);
  POKE(0xF082,last_message>>0);
  POKE(0xF083,last_message>>8);
  POKE(0xF084,message_count>>0);
  POKE(0xF085,message_count>>8);

  // Message 0 is not a real message, so adjust scroll bar accordingly
  draw_scrollbar(0, // SMS thread scroll bar
		 message-1,
		 last_message-1,
		 message_count-1);

  // Note that this is 1-based, not 0-based, because of the reserved message 0
  if (first_message_displayed) *first_message_displayed = message;
  
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
