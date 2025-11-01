/* Shared state structure for MEGAphone

 */


#ifndef SHSTATE_H
#define SHSTATE_H

#include <stdint.h>

#define SHARED_VERSION 0x01
#define SHARED_MAGIC 0xfade

#define SHARED_ADDR 0x0200
#define SHARED_SIZE (0x0400 - 0x0200)


typedef struct shared_state_t {
  unsigned short magic;
  unsigned char version;

  char call_state;
  uint16_t call_contact_id;
#define NUMBER_FIELD_LEN 32
  unsigned char call_state_contact_name[NUMBER_FIELD_LEN+2];
  unsigned char call_state_number[NUMBER_FIELD_LEN+2];
  unsigned char call_state_dtmf_history[NUMBER_FIELD_LEN+2];
  
} Shared;

extern struct shared_state_t shared;

#define shared (*(Shared *)SHARED_ADDR)

void shared_init(void);

#endif
