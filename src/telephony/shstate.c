#include "includes.h"

#include "dialer.h"
#include "shstate.h"

// struct shared_state_t shared;

void shared_init(void)
{  
  if (shared.magic != SHARED_MAGIC) {
    lfill((unsigned long)&shared,0x00,SHARED_SIZE);
    shared.magic = SHARED_MAGIC;
    shared.version = SHARED_VERSION;

    shared.call_state = CALLSTATE_NUMBER_ENTRY;
    shared.call_contact_id = -1;
  }
  if (shared.version != SHARED_VERSION) {
    fail(1);
  }
}
