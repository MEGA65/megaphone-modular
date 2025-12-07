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

#define PAGE_UNKNOWN 0
#define PAGE_SMS_THREAD 1
#define PAGE_CONTACTS 2
uint8_t fonemain_sms_thread_controller(void);
uint8_t fonemain_contact_list_controller(void);

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

  if (af_dirty) {
    af_dirty = 0;

    // For SMS messages, we need to know if the line count has changed
    calc_break_points(buffers.textbox.draft,
		      FONT_UI,
		      RIGHT_AREA_WIDTH_PX, // text field in px
		      RENDER_COLUMNS - 1 - RIGHT_AREA_START_GL);
  }
  
  // Redraw
  af_redraw(active_field,active_field,0);
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

  mega65_uart_print("*** FONECLST entered\r\n");
  
  // Install IRQ animator for waiting
  POKE(0x0314,(uint8_t)(((uint16_t)&irq_wait_animation)>>0));
  POKE(0x0315,(uint8_t)(((uint16_t)&irq_wait_animation)>>8));  

  // Install NMI and BRK catchers
  POKE(0x0316,(uint8_t)(((uint16_t)&brk_catcher)>>0));
  POKE(0x0317,(uint8_t)(((uint16_t)&brk_catcher)>>8));
  POKE(0x0318,(uint8_t)(((uint16_t)&nmi_catcher)>>0));
  POKE(0x0319,(uint8_t)(((uint16_t)&nmi_catcher)>>8));

  asm volatile ( "cli");  
  
  hal_init();

  mount_state_set(MS_CONTACT_LIST, 0);
  read_sector(0,1,0);
  shared.contact_count = record_allocate_next(SECTOR_BUFFER_ADDRESS) - 1;
  
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
  shared.current_page = PAGE_SMS_THREAD;
  loader_exec("FONESMS.PRG");

  // Should never be reached due to loader_exec() above
  return PAGE_SMS_THREAD;
}


uint8_t fonemain_contact_list_controller(void)
{

#define CONTACTS_PER_PAGE 6

  if (shared.redraw) {
    shared.redraw = 0;

    // Ensure contact is visible in listing
    if (shared.position > shared.contact_id) shared.position = shared.contact_id;
    if (shared.position <= (shared.contact_id - CONTACTS_PER_PAGE))
      shared.position = shared.contact_id + 1 - CONTACTS_PER_PAGE;
    if (shared.position<1) shared.position = 1;

    contact_draw_list(shared.position - 1 + CONTACTS_PER_PAGE, shared.contact_id);
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
  case '+': // Create contact
    shared.contact_id = contact_create_new();
    shared.contact_count = shared.contact_id + 1;
    shared.new_contact = 1;
    // FALL THROUGH (dropping into contact edit / SMS thread display)
  case 0xF3: // F3 = switch to contact list
  case 0x20: case 0x0D: // (SPACE or RETURN also does it)
    POKE(0xD610,0); // Remove key event from queue
    return PAGE_SMS_THREAD;
  case 0x11: // Cursor down
  case 0x91: // Cursor up
    if (PEEK(0xD610)==0x11)
      { if ((shared.contact_id+1) < shared.contact_count) shared.contact_id++; }
    else if (shared.contact_id>1) shared.contact_id--;
    // Adjust window so that contact is visible
    if (shared.contact_id >= shared.contact_count)
      shared.contact_id = shared.contact_id-1;

    shared.redraw = 1;
    break;
  case 0x93: // CLR+HOME
    dialpad_clear();
    break;
  }

  if ((PEEK(0xD610)>=0x20 && PEEK(0xD610) < 0x7F)||(PEEK(0xD610)==0x14)) {
    if (shared.active_field == AF_DIALPAD) {
      dialpad_dial_digit(PEEK(0xD610));
    }
  }
    
  POKE(0xD610,0);
  
  return PAGE_CONTACTS;
}
