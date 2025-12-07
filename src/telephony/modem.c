#include "includes.h"

#include "shstate.h"
#include "dialer.h"
#include "screen.h"
#include "records.h"
#include "contacts.h"
#include "buffers.h"
#include "af.h"
#include "modem.h"

void modem_poll(void)
{
  // Check for timeout in state machine
  if ((shared.call_state_timeout != 0)
      && (shared.frame_counter >= shared.call_state_timeout)) {
    shared.call_state_timeout = 0;
    switch(shared.call_state) {
    case CALLSTATE_CONNECTING:
    case CALLSTATE_RINGING:
      // XXX - Send ATH0 to modem
      shared.call_state = CALLSTATE_DISCONNECTED;
      dialpad_draw(shared.active_field, DIALPAD_ALL);
      dialpad_draw_call_state(shared.active_field);
    
      break;      
    }
  }

  // Check for messages from the modem, and process them accordingly
  // RING
  // CONNECTED
  // NO CARRIER
  // SMS RX notification
  // Network time
  // Network signal

  // What else?
}

void modem_place_call(void)
{
  shared.call_state = CALLSTATE_CONNECTING;
  shared.frame_counter = 0;
  shared.call_state_timeout = MODEM_CALL_ESTABLISHMENT_TIMEOUT_SECONDS * FRAMES_PER_SECOND;

  // XXX - Send ATDT to modem

  dialpad_draw(shared.active_field, DIALPAD_ALL);
  dialpad_draw_call_state(shared.active_field);
  
}

void modem_answer_call(void)
{
  switch (shared.call_state) {
  case CALLSTATE_RINGING:
    shared.call_state = CALLSTATE_CONNECTED;

    // XXX - Send ATA to modem

    shared.call_state_timeout = 0;

    dialpad_draw(shared.active_field, DIALPAD_ALL);
    dialpad_draw_call_state(shared.active_field);    
  }
}

void modem_hangup_call(void)
{
  switch (shared.call_state) {
  case CALLSTATE_CONNECTING:
  case CALLSTATE_RINGING:
  case CALLSTATE_CONNECTED:
    shared.call_state = CALLSTATE_DISCONNECTED;
    shared.call_state_timeout = 0;

    // XXX - Send ATH0 to modem

    dialpad_draw(shared.active_field, DIALPAD_ALL);
    dialpad_draw_call_state(shared.active_field);    
  }
}

void modem_mute_call(void)
{
}

void modem_unmute_call(void)
{
}
