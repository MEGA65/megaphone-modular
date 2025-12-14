/* Shared state structure for MEGAphone

 */


#ifndef SHSTATE_H
#define SHSTATE_H

#include <stdint.h>

#include "screen.h"

#define SHARED_VERSION 0x01
#define SHARED_MAGIC 0xfade

#ifdef MEGA65
#define SHARED_ADDR 0xC000
#define SHARED_TOP 0xCFFF
#define SHARED_SIZE (SHARED_TOP + 1 - SHARED_ADDR)
#else
#define SHARED_SIZE (sizeof(struct shared_state_t))
#endif

typedef struct shared_state_t {
  // Gets updated byt irq_wait_animation in hal_asm_llvm.s
  volatile uint16_t frame_counter;

  unsigned short magic;
  unsigned char version;

  char call_state;
  uint16_t call_contact_id;
#define NUMBER_FIELD_LEN 32
  unsigned char call_state_contact_name[NUMBER_FIELD_LEN+2];
  unsigned char call_state_number[NUMBER_FIELD_LEN+2];
  unsigned char call_state_dtmf_history[NUMBER_FIELD_LEN+2];
  uint16_t call_state_timeout;
  char call_state_muted;
  
  struct shared_resource fonts[NUM_FONTS];

  // For FONEMAIN/FONESMS contact list and SMS thread displays
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

  uint8_t current_page;
  uint8_t last_page;
  
  unsigned int first_message_displayed;

// 128KB buffer for 128KB / 256 bytes per glyph = 512 unique unicode glyphs on screen at once
#define GLYPH_DATA_START 0x40000
#define GLYPH_CACHE_SIZE 512
  unsigned long cached_codepoints[GLYPH_CACHE_SIZE];
  unsigned char cached_fontnums[GLYPH_CACHE_SIZE];
  unsigned char cached_glyph_flags[GLYPH_CACHE_SIZE];

  // Modem status
#define MODEM_LINE_SIZE 512
  unsigned char modem_line[MODEM_LINE_SIZE];
  char modem_poll_reset_line;
  uint16_t modem_line_len;
  uint16_t modem_cmgl_counter;
  uint8_t modem_saw_ok;
  uint8_t modem_saw_error;  
  
} Shared;

//extern struct shared_state_t shared;

#ifdef MEGA65
#define shared (*(Shared *)SHARED_ADDR)
#else
extern Shared shared;
#endif

#define PAGE_UNKNOWN 0
#define PAGE_SMS_THREAD 1
#define PAGE_CONTACTS 2
uint8_t fonemain_sms_thread_controller(void);
uint8_t fonemain_contact_list_controller(void);

char shared_init(void);

#endif
