/* Shared state structure for MEGAphone

 */


#ifndef SHSTATE_H
#define SHSTATE_H

#include <stdint.h>

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
  
} Shared;

//extern struct shared_state_t shared;

#define shared (*(Shared *)SHARED_ADDR)

void shared_init(void);

#endif
