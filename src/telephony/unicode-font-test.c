#include <stdio.h>
#include <string.h>

#include "ascii.h"

#include "includes.h"

#include "buffers.h"
#include "screen.h"
#include "records.h"
#include "contacts.h"
#include "smsscreens.h"
#include "sms.h"

unsigned char buffer[128];

unsigned char i;

char hex(unsigned char c)
{
  c&=0xf;
  if (c<0xa) return '0'+c;
  else return 'A'+c-10;
}

char *num_to_str(unsigned int n,char *s)
{
  char *start = s;
  char active=0;
  char c;
  if (n>9999) {
    c='0';
    while(n>9999) { c++; n-=10000; }
    *s = c;
    s++;
    active=1;
  }
  if (n>999||active) {
    c='0';
    while(n>999) { c++; n-=1000; }
    *s = c;
    s++;
    active=1;
  }
  if (n>99||active) {
    c='0';
    while(n>99) { c++; n-=100; }
    *s = c;
    s++;
    active=1;
  }
  if (n>9||active) {
    c='0';
    while(n>9) { c++; n-=10; }
    *s = c;
    s++;
    active=1;
  }
  *s = '0'+n;
  s++;
  *s=0;

  return start;
}

void fatal(const char *file, const char *function, int line, unsigned char r)
{
  POKE(0xD031,0);
  POKE(0xD054,0);
  POKE(0xD016,0xC8);
  POKE(0xD011,0x1B);
  POKE(0xD018,0x21);

  POKE(0x0400,r);
  lcopy((uint32_t)file,0x500,64);
  lcopy((uint32_t)function,0x600,64);
  POKE(0x0401,line);
  POKE(0x0402,line>>8);
  
  while(1) {
    POKE(0xD020,PEEK(0xD012));
  }
}

#ifdef LLVM

int
#else
void
#endif
main(void)
{
  int position;
  char redraw, redraw_draft, reload_contact, erase_draft;
  unsigned char old_draft_line_count;
  unsigned char temp;
  unsigned int contact_ID;
  unsigned char r;
  
  mega65_io_enable();

  // Install NMI/BRK catcher
  POKE(0x0316,(uint8_t)(((uint16_t)&nmi_catcher)>>0));
  POKE(0x0317,(uint8_t)(((uint16_t)&nmi_catcher)>>8));
  POKE(0x0318,(uint8_t)(((uint16_t)&nmi_catcher)>>0));
  POKE(0x0319,(uint8_t)(((uint16_t)&nmi_catcher)>>8));
  POKE(0xFFFE,(uint8_t)(((uint16_t)&nmi_catcher)>>0));
  POKE(0xFFFF,(uint8_t)(((uint16_t)&nmi_catcher)>>8));
  
  screen_setup();  
  screen_clear();    

  generate_rgb332_palette();
  
  // Make sure SD card is idle
  if (PEEK(0xD680)&0x03) {
    POKE(0xD680,0x00);
    POKE(0xD680,0x01);
    while(PEEK(0xD680)&0x3) continue;
    usleep(500000L);
  }

  screen_setup_fonts();

  hal_init();
  
  contact_ID = 3;
  
  position = -1;
  redraw = 1;
  redraw_draft = 1;
  reload_contact = 1;
  erase_draft = 0;

  while(1) {
    unsigned int first_message_displayed;

    if (reload_contact) {
      reload_contact = 0;

      // Clear draft initially
      textbox_erase_draft();
      
      // Mount contact D81s, so that we can retreive draft
      r = mount_contact_qso(contact_ID);
      if (r) fatal(__FILE__,__FUNCTION__,__LINE__,r);
      if (!erase_draft) {
	// Read last record in disk to get any saved draft
	read_record_by_id(0,USABLE_SECTORS_PER_DISK -1,buffers.textbox.draft);
	buffers.textbox.draft_len = strlen((char *)buffers.textbox.draft);
	buffers.textbox.draft_cursor_position = strlen((char *)buffers.textbox.draft) - 1;
	// Reposition cursor to first '|' character in the draft
	// XXX - We really need a better solution than using | as the cursor, but it works for now
	for(buffers.textbox.draft_cursor_position = 0;
	    buffers.textbox.draft_cursor_position<buffers.textbox.draft_len;
	    buffers.textbox.draft_cursor_position++) {
	  if (buffers.textbox.draft[position]=='|') break;
	}
      }
      erase_draft = 0;

      redraw = 1;      
    }
      
    if (redraw) {
      show_busy();
      sms_thread_display(contact_ID,position,0,&first_message_displayed);
      hide_busy();
    }
    redraw=0;

    // Wait for key press
    while(!PEEK(0xD610)) continue;
    switch(PEEK(0xD610)) {
    case 0x0d: // RETURN = send message
      // Don't send empty messages (or that just consist of the cursor)

      buffers_unlock(LOCK_TEXTBOX);

      // XXX Display busy status
      show_busy();
      sms_send_to_contact(contact_ID,buffers.textbox.draft);
      buffers_unlock(LOCK_TELEPHONY);      

      // Sending to a contact unmounts the thread, so we need to fix that
      try_or_fail(mount_contact_qso(contact_ID));
      hide_busy();
      
      reload_contact = 1;
      erase_draft = 1;
      
      break;
    case 0x11: // down arrow
      if (position<-1) { redraw=1; position++; }
      break;
    case 0x14: // DELETE
      if (buffers.textbox.draft_cursor_position) {
	lcopy((uint32_t)&buffers.textbox.draft[buffers.textbox.draft_cursor_position],
	      (uint32_t)&buffers.textbox.draft[buffers.textbox.draft_cursor_position-1],
	      RECORD_DATA_SIZE - (buffers.textbox.draft_cursor_position));
	buffers.textbox.draft_cursor_position--;
	buffers.textbox.draft_len--;

	redraw_draft=1;
      }
      break;
      
    case 0x93: // CLR+HOME
      textbox_erase_draft();

      redraw_draft=1;      
      break;
    case 0x91: // up arrow
      if (first_message_displayed>1) { redraw=1; position--; }
      break;
    case 0x1d: // cursor right
      if (buffers.textbox.draft_cursor_position < (buffers.textbox.draft_len-1)) {
	// XXX swap cursor byte with _codepoint_ to the right.
	// This may require shifting more than 1 byte.
	// For now, we just assume only ASCII chars, and move position 1 byte at a time.
	temp = buffers.textbox.draft[buffers.textbox.draft_cursor_position+1];
	buffers.textbox.draft[buffers.textbox.draft_cursor_position+1] = '|';
	buffers.textbox.draft[buffers.textbox.draft_cursor_position] = temp;
	buffers.textbox.draft_cursor_position++;
	redraw_draft = 1;
      }
      break;
    case 0x9d: // cursor left
      if (buffers.textbox.draft_cursor_position > 0) {
	// XXX swap cursor byte with _codepoint_ to the right.
	// This may require shifting more than 1 byte.
	// For now, we just assume only ASCII chars, and move position 1 byte at a time.
	temp = buffers.textbox.draft[buffers.textbox.draft_cursor_position-1];
	buffers.textbox.draft[buffers.textbox.draft_cursor_position-1] = '|';
	buffers.textbox.draft[buffers.textbox.draft_cursor_position] = temp;
	buffers.textbox.draft_cursor_position--;
	redraw_draft = 1;
      }
      break;
    }
    if (PEEK(0xD610)>=0x20 && PEEK(0xD610) < 0x7F) {
      // It's a character to add to our draft message
      if (buffers.textbox.draft_len < (RECORD_DATA_SIZE-1) ) {
	// Shuffle from cursor
	lcopy((uint32_t)&buffers.textbox.draft[buffers.textbox.draft_cursor_position],
	      (uint32_t)&buffers.textbox.draft[buffers.textbox.draft_cursor_position+1],
	      RECORD_DATA_SIZE - (1 + buffers.textbox.draft_cursor_position));
	buffers.textbox.draft[buffers.textbox.draft_cursor_position]=PEEK(0xD610);
	buffers.textbox.draft_cursor_position++;
	buffers.textbox.draft_len++;

	redraw_draft = 1;
      }
    }

    if (redraw_draft) {
      redraw_draft = 0;

      // Update saved draft in the D81
      write_record_by_id(0,USABLE_SECTORS_PER_DISK -1, buffers.textbox.draft);
      
      calc_break_points(buffers.textbox.draft,
			FONT_UI,
			294, // text field in px
			  RENDER_COLUMNS - 1 - 45);      
      
      // Only redraw the message draft if it hasn't changed how many lines it uses
      if (buffers.textbox.line_count == old_draft_line_count ) {
	textbox_draw(360/8,
		     MAX_ROWS - buffers.textbox.line_count,
		     360,
		     294,
		     RENDER_COLUMNS - 1 - 45,
		     FONT_UI,
		     0x8F,
		     buffers.textbox.draft,
		     0,
		     buffers.textbox.line_count-1,
		     VIEWPORT_PADDED);
      } else {
	redraw = 1;
      }

      old_draft_line_count = buffers.textbox.line_count;      
      
    }
    // Acknowledge key press
    POKE(0xD610,0);
  }
  
  return 0;
}
