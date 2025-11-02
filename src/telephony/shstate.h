/* Shared state structure for MEGAphone

 */


#ifndef SHSTATE_H
#define SHSTATE_H

#include <stdint.h>

#include "screen.h"

#define SHARED_VERSION 0x01
#define SHARED_MAGIC 0xfade

#define SHARED_ADDR 0x033C
#define SHARED_TOP 0x03FF
// XXX If we get rid of the KERNAL, we can use from $0200 -- $03FF
// But for now, we have to not stomp the NMI handler address.
#define SHARED_SIZE (SHARED_TOP + 1 - SHARED_ADDR)


typedef struct shared_state_t {
  // Gets updated byt irq_wait_animation in hal_asm_llvm.s
  volatile unsigned char frame_counter;

  unsigned short magic;
  unsigned char version;

  char call_state;
  uint16_t call_contact_id;
#define NUMBER_FIELD_LEN 32
  unsigned char call_state_contact_name[NUMBER_FIELD_LEN+2];
  unsigned char call_state_number[NUMBER_FIELD_LEN+2];
  unsigned char call_state_dtmf_history[NUMBER_FIELD_LEN+2];

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
  
} Shared;

//extern struct shared_state_t shared;

#define shared (*(Shared *)SHARED_ADDR)

#define PAGE_UNKNOWN 0
#define PAGE_SMS_THREAD 1
#define PAGE_CONTACTS 2
uint8_t fonemain_sms_thread_controller(void);
uint8_t fonemain_contact_list_controller(void);

void shared_init(void);

#endif
