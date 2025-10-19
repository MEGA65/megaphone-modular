#include <stdio.h>
#include <string.h>

#include "ascii.h"

#include "includes.h"

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

#define PAGE_UNKNOWN 0
#define PAGE_SMS_THREAD 1
#define PAGE_CONTACTS 2
uint8_t fonemain_sms_thread_controller(void);
uint8_t fonemain_contact_list_controller(void);


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

extern void irq_wait_animation(void);

void save_and_redraw_active_field(int8_t active_field, uint16_t contact_id)
{
  uint16_t cursor_stash = buffers.textbox.draft_cursor_position;
  // Remove cursor
  textbox_remove_cursor();
  // Save changes
  af_store(active_field,contact_id);
  // Reinsert cursor
  textbox_insert_cursor(cursor_stash);
  
  // Redraw
  af_redraw(active_field,active_field,0);
} 


// Global state for telephony software
int16_t position;
char redraw, redraw_draft, reload_contact, erase_draft;
char redraw_contact;
unsigned char old_draft_line_count;
unsigned char temp;
int16_t contact_id;
int16_t contact_count;
unsigned char r;
// active field needs to be signed, so that we can wrap field numbers
int8_t active_field;
int8_t prev_active_field;

unsigned int first_message_displayed;

void reset_view(void)
{
  position = -1;
  redraw = 1;
  reload_contact = 1;
  redraw_contact = 1;
  erase_draft = 0;
  first_message_displayed = -1;

  active_field = 0;
}

int
#else
void
#endif
main(void)
{
  
  mega65_io_enable();

  asm volatile ( "sei");  

  // Install IRQ animator for waiting
  POKE(0x0314,(uint8_t)(((uint16_t)&irq_wait_animation)>>0));
  POKE(0x0315,(uint8_t)(((uint16_t)&irq_wait_animation)>>8));  
  
  // Install NMI/BRK catcher
  POKE(0x0316,(uint8_t)(((uint16_t)&nmi_catcher)>>0));
  POKE(0x0317,(uint8_t)(((uint16_t)&nmi_catcher)>>8));
  POKE(0x0318,(uint8_t)(((uint16_t)&nmi_catcher)>>0));
  POKE(0x0319,(uint8_t)(((uint16_t)&nmi_catcher)>>8));
  POKE(0xFFFE,(uint8_t)(((uint16_t)&nmi_catcher)>>0));
  POKE(0xFFFF,(uint8_t)(((uint16_t)&nmi_catcher)>>8));

  asm volatile ( "cli");  
  
  screen_setup();
  screen_clear();
  statusbar_setup();

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

  mount_state_set(MS_CONTACT_LIST, 0);
  read_sector(0,1,0);
  contact_count = record_allocate_next(SECTOR_BUFFER_ADDRESS) - 1;

  // Start with contact list
  uint8_t current_page = PAGE_CONTACTS;
  uint8_t last_page = PAGE_UNKNOWN;   
  contact_id = 2;
  
  reset_view();

  af_retrieve(active_field, active_field, contact_id);
  
  dialpad_draw(active_field);  
  
  statusbar_draw();
  
  show_busy();

  while(1) {
    // Reload and redraw things as required when changing views.
    if (current_page != last_page) {
      reset_view();
    }
    last_page = current_page;
    switch (current_page) {
    case PAGE_SMS_THREAD:
      current_page = fonemain_sms_thread_controller(); break;
    case PAGE_CONTACTS:    current_page = fonemain_contact_list_controller(); break;
    default:
      // If something goes wrong, go back to contact list.
      current_page = PAGE_CONTACTS;
    }
  }
  
  return 0;
}
    
uint8_t fonemain_sms_thread_controller(void)
{  
  if (reload_contact) {
    reload_contact = 0;
    
    // Redisplay contact at top of screen
    
    // Mount contacts 
    if (!contact_read(contact_id,buffers.textbox.contact_record)) {
      redraw_contact = 1;
    }
    
    // Clear draft initially
    textbox_erase_draft();
    
    // Mount contact D81s, so that we can retreive draft
    r = mount_contact_qso(contact_id);
    if (r) fatal(__FILE__,__FUNCTION__,__LINE__,r);
    if (!erase_draft) {
      // Read last record in disk to get any saved draft
      read_record_by_id(0,USABLE_SECTORS_PER_DISK -1,buffers.textbox.draft);
      textbox_find_cursor();
      
    }
    erase_draft = 0;
    
    redraw = 1;      
  }
  
  if (redraw_contact) {
    redraw_contact = 0;
    contact_draw(RIGHT_AREA_START_GL, 3,
		 RIGHT_AREA_START_PX,
		 RENDER_COLUMNS - 1 - RIGHT_AREA_START_GL,
		 // Subtract a bit of space for scroll bar etc
		 RIGHT_AREA_WIDTH_PX - 16,
		 contact_id,
		 active_field // which field is currently active/highlighted
		 );
  }       
  
  if (redraw) {
    sms_thread_display(contact_id,position,
		       1, // Show message edit box
		       active_field,
		       &first_message_displayed);
  }
  redraw=0;
  
  // Wait for key press: This is the only time that we aren't "busy"    
  hide_busy();
  while(!PEEK(0xD610)) {
    // Keep the clock updated
    statusbar_draw_time();
    continue;
  }
  show_busy();
  
  switch(PEEK(0xD610)) {
  case 0xF3: // F3 = switch to contact list
    POKE(0xD610,0); // Remove F3 key event from queue
    // Highlight the contact we have been reading the messages of
    position = contact_id - 2;
    if (position < 0) position = 0;
    return PAGE_CONTACTS;
  case 0x09: case 0x0F: // TAB - move fields
    
    // Redraw old field sans cursor or selection
    textbox_remove_cursor();
    // Save any changes to the field before TABing to next field
    af_store(active_field,contact_id);
    af_redraw(AF_NONE,active_field,0);
    
    prev_active_field = active_field;
    if (PEEK(0xD610)==0x0F) active_field--; // shift+tab = 0x0f
    else active_field++;
    if (active_field > AF_MAX ) active_field = 0;
    if (active_field < 0 ) active_field = AF_MAX;
    
    // Now redraw what we need to
    if (active_field == AF_DIALPAD || prev_active_field == AF_DIALPAD ) {
      dialpad_draw(active_field);	
    }
    
    // Load draft with the correct field
    af_retrieve(active_field, active_field, contact_id);
    // textbox_find_cursor();
    af_redraw(active_field,active_field,0);
    
    break;
  case 0x0d: // RETURN = send message
    // Don't send empty messages (or that just consist of the cursor)
    
    if (active_field==AF_SMS) {
      buffers_unlock(LOCK_TEXTBOX);
      
      textbox_remove_cursor();
      sms_send_to_contact(contact_id,buffers.textbox.draft);
      buffers_unlock(LOCK_TELEPHONY);      
      
      // Sending to a contact unmounts the thread, so we need to fix that
      try_or_fail(mount_contact_qso(contact_id));
      
      // Clear saved draft
      textbox_erase_draft();
      write_record_by_id(0,USABLE_SECTORS_PER_DISK -1, buffers.textbox.draft);
      
      reload_contact = 1;
      erase_draft = 1;
    }
    break;
  case 0x11: // down arrow
    
    // Save any changes to active field before redrawing causes it to be reloaded
    textbox_remove_cursor();
    af_store(active_field,contact_id);            
    
    if (position<-1) { redraw=1; position++; }
    break;
  case 0x14: // DELETE
    if (buffers.textbox.draft_cursor_position) {
      lcopy((uint32_t)&buffers.textbox.draft[buffers.textbox.draft_cursor_position],
	    (uint32_t)&buffers.textbox.draft[buffers.textbox.draft_cursor_position-1],
	    RECORD_DATA_SIZE - (buffers.textbox.draft_cursor_position));
      buffers.textbox.draft_cursor_position--;
      buffers.textbox.draft_len--;
      
      save_and_redraw_active_field(active_field, contact_id);
    }
    break;
    
  case 0x94: // SHIFT+DEL = delete last message in the thread.
    sms_delete_message(contact_id,
		       -1 // last message in the thread
		       );
    redraw = 1;
    break;
    
  case 0x93: // CLR+HOME
    textbox_erase_draft();
    af_store(active_field, contact_id);
    
    redraw_draft=1;      
    break;
  case 0x91: // up arrow
    
    // Save any changes to active field before redrawing causes it to be reloaded
    textbox_remove_cursor();
    af_store(active_field,contact_id);            
    
    if (first_message_displayed>1) { redraw=1; position--; }
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
      redraw_draft = 1;
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
      
      save_and_redraw_active_field(active_field, contact_id);
      
      redraw_draft = 1;
    }
  }
  
  if (redraw_draft) {
    redraw_draft = 0;
    
    // For SMS messages, we need to know if the line count has changed
    calc_break_points(buffers.textbox.draft,
		      FONT_UI,
		      RIGHT_AREA_WIDTH_PX, // text field in px
		      RENDER_COLUMNS - 1 - RIGHT_AREA_START_GL);
    
    // Only redraw the message draft if it hasn't changed how many lines it uses
    if (buffers.textbox.line_count == old_draft_line_count ) {
      af_redraw(active_field,active_field,0);
    } else {
      redraw = 1;
    }
    
    old_draft_line_count = buffers.textbox.line_count;      
    
  }
  // Acknowledge key press
  POKE(0xD610,0);

  return PAGE_SMS_THREAD;
}

uint8_t fonemain_contact_list_controller(void)
{

#define CONTACTS_PER_PAGE 6
    
  if (redraw) {
    redraw = 0;

    // Ensure contact is visible in listing
    if (position > contact_id) position = contact_id;
    if (position <= (contact_id - CONTACTS_PER_PAGE))
      position = contact_id + 1 - CONTACTS_PER_PAGE;
    if (position<1) position = 1;

    contact_draw_list(position - 1 + CONTACTS_PER_PAGE, contact_id);
  }

  // Wait for key press: This is the only time that we aren't "busy"    
  hide_busy();
  while(!PEEK(0xD610)) {
    // Keep the clock updated
    statusbar_draw_time();
    continue;
  }
  show_busy();

  switch(PEEK(0xD610)) {
  case 0xF3: // F3 = switch to contact list
  case 0x20: case 0x0D: // (SPACE or RETURN also does it)
    POKE(0xD610,0); // Remove F3 key event from queue
    return PAGE_SMS_THREAD;
  case 0x11: // Cursor down
  case 0x91: // Cursor up
    if (PEEK(0xD610)==0x11)
      { if ((contact_id+1) < contact_count) contact_id++; }
    else if (contact_id>1) contact_id--;
    // Adjust window so that contact is visible
    if (contact_id >= contact_count) contact_id = contact_id-1;

    redraw = 1;
    break;
  }
  POKE(0xD610,0);
  
  return PAGE_CONTACTS;
}
