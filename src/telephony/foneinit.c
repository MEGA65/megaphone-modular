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

unsigned int first_message_displayed;

void reset_view(uint8_t current_page)
{
  position = -1;
  redraw = 1;
  reload_contact = 1;
  redraw_contact = 1;
  erase_draft = 0;
  first_message_displayed = -1;
  old_draft_line_count = -1;
  
  active_field = AF_DIALPAD;

  // For convenience, highlight the first contact field on creation
  if (new_contact) { active_field = 2; new_contact=0; }
  if (current_page==PAGE_SMS_THREAD) { active_field = AF_SMS; }
  dialpad_hide_show_cursor(active_field);
}

int
#else
void
#endif
main(void)
{
  
  mega65_io_enable();

  shared_init(); 
    
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

  // Chain to fonemain
  
  return 0;
}
    
