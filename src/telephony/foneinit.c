#include <stdio.h>
#include <string.h>

#include "ascii.h"

#include "includes.h"

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

#include "loader.h"

unsigned char i;

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
uint8_t new_contact;

int
#else
void
#endif
main(void)
{
  
  mega65_io_enable();

  shared_init(); 

  mega65_uart_print("*** FONEINIT entered\r\n");
  
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
  shared.contact_count = record_allocate_next(SECTOR_BUFFER_ADDRESS) - 1;

  // Start with contact list
  shared.current_page = PAGE_CONTACTS;
  shared.last_page = PAGE_UNKNOWN;   
  shared.contact_id = 2;
  shared.new_contact = 0;

  reset_view(shared.current_page);
    
  af_retrieve(shared.active_field, shared.active_field, shared.contact_id);

  dialpad_set_call_state(CALLSTATE_NUMBER_ENTRY);
  dialpad_hide_show_cursor(shared.active_field);
  dialpad_draw(shared.active_field,DIALPAD_ALL);  
  dialpad_draw_call_state(shared.active_field);
  
  statusbar_draw();
  
  // Chain to contact list
  loader_exec("FONECLST.PRG");

  return 0;
}
    
