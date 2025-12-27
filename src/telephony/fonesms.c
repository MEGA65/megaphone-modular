#include <stdio.h>
#include <string.h>

#include "ascii.h"

#include "includes.h"

#include "loader.h"
#include "shstate.h"
#include "buffers.h"
#include "screen.h"
#include "records.h"
#include "contacts.h"
#include "smsscreens.h"
#include "contactscreens.h"
#include "sms.h"
#include "dialer.h"
#include "status.h"
#include "af.h"
#include "mountstate.h"
#include "modem.h"

unsigned char i;

unsigned char temp;
unsigned char r;

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

extern void irq_wait_animation(void);

void save_and_redraw_active_field(int8_t active_field, uint16_t contact_id)
{
  uint16_t cursor_stash = buffers.textbox.draft_cursor_position;
  // Remove cursor
  textbox_remove_cursor();
  // Save changes
  af_store(shared.active_field,contact_id);
  // Reinsert cursor
  textbox_insert_cursor(cursor_stash);

  if (af_dirty) {
    af_dirty = 0;

    // For SMS messages, we need to know if the line count has changed
    calc_break_points(buffers.textbox.draft,
		      FONT_UI,
		      RIGHT_AREA_WIDTH_PX, // text field in px
		      RENDER_COLUMNS - 1 - RIGHT_AREA_START_GL);
  }
  
  // Redraw
  af_redraw(shared.active_field,active_field,0);
} 

void reset_view(uint8_t current_page)
{
  shared.position = -1;
  shared.redraw = 1;
  shared.reload_contact = 1;
  shared.redraw_contact = 1;
  shared.erase_draft = 0;
  shared.first_message_displayed = -1;
  shared.old_draft_line_count = -1;
  
  shared.active_field = AF_DIALPAD;

  // For convenience, highlight the first contact field on creation
  if (shared.new_contact) { shared.active_field = 2; shared.new_contact=0; }
  if (current_page==PAGE_SMS_THREAD) { shared.active_field = AF_SMS; }
  dialpad_hide_show_cursor(shared.active_field);
}

int
#else
void
#endif
main(void)
{

  mega65_io_enable();

  shared_init(); 
  
  asm volatile ( "sei");  

  mega65_uart_print("*** FONESMS entered\r\n");  
  
  // Install IRQ animator for waiting
  POKE(0x0314,(uint8_t)(((uint16_t)&irq_wait_animation)>>0));
  POKE(0x0315,(uint8_t)(((uint16_t)&irq_wait_animation)>>8));  
  
  // Install NMI and BRK catchers
  POKE(0x0316,(uint8_t)(((uint16_t)&brk_catcher)>>0));
  POKE(0x0317,(uint8_t)(((uint16_t)&brk_catcher)>>8));
  POKE(0x0318,(uint8_t)(((uint16_t)&nmi_catcher)>>0));
  POKE(0x0319,(uint8_t)(((uint16_t)&nmi_catcher)>>8));

  asm volatile ( "cli");  
  
  show_busy();

  while(1) {
    // Reload and redraw things as required when changing views.
    if (shared.current_page != shared.last_page) {
      reset_view(shared.current_page);

      dialpad_hide_show_cursor(shared.active_field);
      dialpad_draw(shared.active_field,DIALPAD_ALL);  
      dialpad_draw_call_state(shared.active_field);      
    }
    shared.last_page = shared.current_page;
    switch (shared.current_page) {
    case PAGE_SMS_THREAD:
      shared.current_page = fonemain_sms_thread_controller(); break;
    case PAGE_CONTACTS:
      shared.current_page = fonemain_contact_list_controller(); break;
    default:
      // If something goes wrong, go back to contact list.
      shared.current_page = PAGE_CONTACTS;
    }
  }
  
  return 0;
}
    
uint8_t fonemain_sms_thread_controller(void)
{  
  if (shared.reload_contact) {
    shared.reload_contact = 0;
    
    // Redisplay contact at top of screen
    
    // Mount contacts 
    if (!contact_read(shared.contact_id,buffers.textbox.contact_record)) {
      shared.redraw_contact = 1;
    }
    
    // Clear draft initially
    textbox_erase_draft();
    
    // Mount contact D81s, so that we can retreive draft
    r = mount_contact_qso(shared.contact_id);
    if (r) fatal(__FILE__,__FUNCTION__,__LINE__,r);
    if (!shared.erase_draft) {
      // Read last record in disk to get any saved draft
      read_record_by_id(0,USABLE_SECTORS_PER_DISK -1,buffers.textbox.draft);
      textbox_find_cursor();
      
    }
    shared.erase_draft = 0;
    
    shared.redraw = 1;      
  }
  
  if (shared.redraw_contact) {
    shared.redraw_contact = 0;
    contact_draw(RIGHT_AREA_START_GL, 3,
		 RIGHT_AREA_START_PX,
		 RENDER_COLUMNS - 1 - RIGHT_AREA_START_GL,
		 // Subtract a bit of space for scroll bar etc
		 RIGHT_AREA_WIDTH_PX - 16,
		 shared.contact_id,
		 shared.active_field // which field is currently active/highlighted
		 );
  }       
  
  if (shared.redraw) {
    sms_thread_display(shared.contact_id, shared.position,
		       1, // Show message edit box
		       shared.active_field,
		       &shared.first_message_displayed);
    // And make sure we have the current field retrieved after
    af_retrieve(shared.active_field, shared.active_field, shared.contact_id);
  }
  shared.redraw=0;
  
  // Wait for key press: This is the only time that we aren't "busy"    
  hide_busy();
  while(!PEEK(0xD610)) {
    // Keep the clock updated
    statusbar_draw_time();
    modem_poll();
    continue;
  }
  show_busy();
  
  switch(PEEK(0xD610)) {
  case 0xF1: // Select contact to dial -- but only if call state allows it.
    dialer_dial_contact();
    break;
  case 0xF5: // toggle mute
    modem_toggle_mute();
    break;
  case 0xF7: // Hang up
    modem_hangup_call();
    break;
  case 0x1F: // HELP key
    break;
  case 0xF3: // F3 = switch to contact list
    POKE(0xD610,0); // Remove F3 key event from queue
    // Highlight the contact we have been reading the messages of
    shared.position = shared.contact_id - 2;
    if (shared.position < 0) shared.position = 0;
    return PAGE_CONTACTS;
  case 0x09: case 0x0F: // TAB - move fields
    
    // Redraw old field sans cursor or selection
    textbox_remove_cursor();
    // Save any changes to the field before TABing to next field
    af_store(shared.active_field,shared.contact_id);
    af_redraw(AF_NONE,shared.active_field,0);
    
    shared.prev_active_field = shared.active_field;
    if (PEEK(0xD610)==0x0F) shared.active_field--; // shift+tab = 0x0f
    else shared.active_field++;
    if (shared.active_field > AF_MAX ) shared.active_field = 0;
    if (shared.active_field < 0 ) shared.active_field = AF_MAX;

    dialpad_hide_show_cursor(shared.active_field);
    
    // Now redraw what we need to
    if (shared.active_field == AF_DIALPAD || shared.prev_active_field == AF_DIALPAD ) {
      dialpad_draw(shared.active_field,DIALPAD_ALL);
      // Update dialer field highlighting as required
      dialpad_draw_call_state(shared.active_field);
    }
    
    // Load draft with the correct field
    af_retrieve(shared.active_field, shared.active_field, shared.contact_id);
    // textbox_find_cursor();
    af_redraw(shared.active_field, shared.active_field,0);
    
    break;
  case 0x0d: // RETURN = send message
    // Don't send empty messages (or that just consist of the cursor)
    
    if (shared.active_field==AF_SMS) {
      buffers_unlock(LOCK_TEXTBOX);
      
      textbox_remove_cursor();
      sms_send_to_contact(shared.contact_id,buffers.textbox.draft);
      buffers_unlock(LOCK_TELEPHONY);      
      
      // Sending to a contact unmounts the thread, so we need to fix that
      try_or_fail(mount_contact_qso(shared.contact_id));
      
      // Clear saved draft
      textbox_erase_draft();
      write_record_by_id(0,USABLE_SECTORS_PER_DISK -1, buffers.textbox.draft);
      
      shared.reload_contact = 1;
      shared.erase_draft = 1;
    }
    break;
  case 0x11: // down arrow
    
    // Save any changes to active field before redrawing causes it to be reloaded
    textbox_remove_cursor();
    af_store(shared.active_field,shared.contact_id);            
    
    if (shared.position<-1) { shared.redraw=1; shared.position++; }
    break;
  case 0x14: // DELETE
    if (shared.active_field != AF_DIALPAD) {
      if (buffers.textbox.draft_cursor_position) {
	lcopy((uint32_t)&buffers.textbox.draft[buffers.textbox.draft_cursor_position],
	      (uint32_t)&buffers.textbox.draft[buffers.textbox.draft_cursor_position-1],
	      RECORD_DATA_SIZE - (buffers.textbox.draft_cursor_position));
	buffers.textbox.draft_cursor_position--;
	buffers.textbox.draft_len--;
	
	save_and_redraw_active_field(shared.active_field, shared.contact_id);
      }
    } else {
      dialpad_dial_digit(PEEK(0xD610));
    }
    break;
    
  case 0x94: // SHIFT+DEL = delete last message in the thread.
    sms_delete_message(shared.contact_id,
		       -1 // last message in the thread
		       );
    shared.redraw = 1;
    break;
    
  case 0x93: // CLR+HOME
    if (shared.active_field==AF_DIALPAD) dialpad_clear();
    else {
      textbox_erase_draft();
      af_store(shared.active_field, shared.contact_id);
    }
    shared.redraw_draft=1;      
    break;
  case 0x91: // up arrow
    
    // Save any changes to active field before redrawing causes it to be reloaded
    textbox_remove_cursor();
    af_store(shared.active_field,shared.contact_id);            
    
    if (shared.first_message_displayed>1) { shared.redraw=1; shared.position--; }
    break;
  case 0x1d: // cursor right
    if (buffers.textbox.draft_cursor_position < (buffers.textbox.draft_len-1)) {
      // XXX swap cursor byte with _codepoint_ to the right.
      // This may require shifting more than 1 byte.
      // For now, we just assume only ASCII chars, and move position 1 byte at a time.
      temp = buffers.textbox.draft[buffers.textbox.draft_cursor_position+1];
      buffers.textbox.draft[buffers.textbox.draft_cursor_position+1] = CURSOR_CHAR;
      buffers.textbox.draft[buffers.textbox.draft_cursor_position] = temp;
      buffers.textbox.draft_cursor_position++;
      shared.redraw_draft = 1;
    }
    break;
  case 0x9d: // cursor left
    if (buffers.textbox.draft_cursor_position > 0) {
      // XXX swap cursor byte with _codepoint_ to the right.
      // This may require shifting more than 1 byte.
      // For now, we just assume only ASCII chars, and move position 1 byte at a time.
      temp = buffers.textbox.draft[buffers.textbox.draft_cursor_position-1];
      buffers.textbox.draft[buffers.textbox.draft_cursor_position-1] = CURSOR_CHAR;
      buffers.textbox.draft[buffers.textbox.draft_cursor_position] = temp;
      buffers.textbox.draft_cursor_position--;
      shared.redraw_draft = 1;
    }
    break;
  }
  if (PEEK(0xD610)>=0x20 && PEEK(0xD610) < 0x7F) {
    // It's a character to add to our draft message
    if (shared.active_field == AF_DIALPAD) {
      dialpad_dial_digit(PEEK(0xD610));
    } else if (buffers.textbox.draft_len < (RECORD_DATA_SIZE-1) ) {
      
      // Shuffle from cursor
      lcopy((uint32_t)&buffers.textbox.draft[buffers.textbox.draft_cursor_position],
	    (uint32_t)&buffers.textbox.draft[buffers.textbox.draft_cursor_position+1],
	    RECORD_DATA_SIZE - (1 + buffers.textbox.draft_cursor_position));
      buffers.textbox.draft[buffers.textbox.draft_cursor_position]=PEEK(0xD610);
      buffers.textbox.draft_cursor_position++;
      buffers.textbox.draft_len++;
      
      save_and_redraw_active_field(shared.active_field, shared.contact_id);
      
      shared.redraw_draft = 1;
    }
  }
  
  if (shared.redraw_draft) {
    shared.redraw_draft = 0;

    // For SMS messages, we need to know if the line count has changed
    calc_break_points(buffers.textbox.draft,
		      FONT_UI,
		      RIGHT_AREA_WIDTH_PX, // text field in px
		      RENDER_COLUMNS - 1 - RIGHT_AREA_START_GL);

    // Only redraw the message draft if it hasn't changed how many lines it uses
    if (buffers.textbox.line_count == shared.old_draft_line_count ) {
      af_redraw(shared.active_field,shared.active_field,0);
    } else {
      shared.redraw = 1;
    }
    
    shared.old_draft_line_count = buffers.textbox.line_count;      
    
  }
  // Acknowledge key press
  POKE(0xD610,0);
  
  return PAGE_SMS_THREAD;
}


uint8_t fonemain_contact_list_controller(void)
{
  shared.current_page = PAGE_CONTACTS;
  loader_exec("FONECLST.PRG");

  // Should never be reached due to loader_exec() call
  return PAGE_CONTACTS;
}
