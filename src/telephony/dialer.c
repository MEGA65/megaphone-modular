#include "includes.h"

#include "dialer.h"
#include "screen.h"
#include "af.h"

char call_state = CALLSTATE_NUMBER_ENTRY;
uint16_t call_contact_id = -1;
unsigned char call_state_contact_name[32]={0};
unsigned char call_state_number[32]={0};
unsigned char call_state_dtmf_history[32]={0};

// XXX Use the fact that chip RAM at 0x60000 reads as zeroes :)
#define DIALPAD_BLANK_GLYPH_ADDR 0x60000

static unsigned char MSG_DIAL[]
= "Dial or select contact";
static unsigned char MSG_CALLING[]
= "Calling...";
static unsigned char MSG_INCOMING[]
= "Incoming Call...";
static unsigned char MSG_EMPTY[]
= "";
static unsigned char MSG_INCALL[]
= "In Call ";
static unsigned char MSG_ENDED[]
= "Call Ended ";

unsigned char *call_state_messages[CALLSTATE_MAX+1]={
  MSG_DIAL,
  MSG_CALLING,
  MSG_INCOMING,
  MSG_INCOMING,
  MSG_ENDED,
  MSG_DIAL
};

#define CALL_STATE_LINES 6
#define CALL_STATE_LINE_DIALED_NUMBER 3
#define CALL_STATE_LINE_DTMF_HISTORY 5
uint8_t call_state_colours[CALL_STATE_LINES]={
  0x01,
  0x06,
  0x0e,
  0x81,
  0x06,
  0x81
};

void dialpad_draw_call_state(char active_field)
{
  if (call_state>CALLSTATE_MAX) call_state = CALLSTATE_IDLE;

  unsigned char *s;
  
  // Neither number field is active by default
  call_state_colours[CALL_STATE_LINE_DIALED_NUMBER]=0x8c;
  call_state_colours[CALL_STATE_LINE_DTMF_HISTORY]=0x8c;

  if (active_field==AF_DIALPAD) {
    if (call_state == CALLSTATE_CONNECTED)
      call_state_colours[CALL_STATE_LINE_DTMF_HISTORY]=0x81;
    else   
      call_state_colours[CALL_STATE_LINE_DIALED_NUMBER]=0x81;
  }
  
  for(uint8_t l=0;l<CALL_STATE_LINES;l++) {
    s = MSG_EMPTY;
    switch(l) {
    case 0: s = call_state_messages[(uint8_t)call_state]; break;
    case 2: s = call_state_contact_name; break;
    case 3: s = call_state_number; break;
    case 5: s = call_state_dtmf_history; break;
    }
    //    mega65_uart_printptr(s);
    draw_string_nowrap(2,3+l,
		       FONT_UI,
		       call_state_colours[l],
		       s,
		       16,
		       RIGHT_AREA_START_PX - 16 - 40,
		       RIGHT_AREA_START_GL,
		       NULL,
		       VIEWPORT_PADDED_RIGHT,
		       NULL,
		       NULL);
  }

  
}
  
void dialpad_set_call_state(char new_state)
{

  if (call_state>CALLSTATE_MAX) call_state = CALLSTATE_IDLE;
  
  
  switch(new_state) {
  case CALLSTATE_NUMBER_ENTRY:
    break;
  case CALLSTATE_CONNECTING:
    break;
  case CALLSTATE_CONNECTED:
    if (call_state == CALLSTATE_RINGING) {
      // XXX Log accepted inbound call call  "➡️☎️ at XYZ"
    }
    if (call_state == CALLSTATE_CONNECTING) {      
      // XXX Log successful outbound call "☎️➡️ at XYZ"
    }
    break;
  case CALLSTATE_DISCONNECTED:
    switch(call_state) {
    case CALLSTATE_CONNECTING:
      // XXX Log failed/rejected call "☎️❌ at XYZ"
      break;
    case CALLSTATE_RINGING:
      // XXX Add 1 to missed-call counter
      // XXX Log missed call "↩️☎️ at XYZ"
      break;
    case CALLSTATE_CONNECTED:
      // End call.
      // XXX Log call duration "☎️⬇️ at XYZ"
      break;
    }
    break;
  default:
  case CALLSTATE_IDLE:
    if (call_state != CALLSTATE_IDLE) {
      // Erase contact info
      call_contact_id = -1;
      call_state_contact_name[0]=0;
      call_state_number[0]=0;
      call_state_dtmf_history[0]=0;
    }
    break;
  }

  call_state = new_state;		     
  
}

void dialpad_draw_button(unsigned char symbol_num,
			 unsigned char x, unsigned char y,
			 unsigned char colour)
{
  unsigned long screen_ram_addr = screen_ram + y * 0x200 + x*2;
  unsigned long colour_ram_addr = colour_ram + y * 0x200 + x*2;

  unsigned long glyph_num_addr = 0x10000L + symbol_num * 8;

  unsigned int glyph_num = DIALPAD_BLANK_GLYPH_ADDR / 64;
  
  for(uint8_t yy=0;yy<4;yy++) {
    for(uint8_t xx=0;xx<4;xx++) {

      if (yy==0||yy==3) glyph_num = DIALPAD_BLANK_GLYPH_ADDR / 64;
      else {
	if (xx) glyph_num = lpeek(glyph_num_addr - 1);
	else glyph_num=0xa0;
	if (glyph_num!=0xa0) {
	  glyph_num = glyph_num*2 + (0x10080 / 64);
	} else glyph_num = DIALPAD_BLANK_GLYPH_ADDR / 64;
	glyph_num_addr++;
      }
      
      lpoke(screen_ram_addr + 0, glyph_num & 0xff);
      lpoke(screen_ram_addr + 1, glyph_num >>8);
      lpoke(colour_ram_addr + 0, 0x28);
      lpoke(colour_ram_addr + 1, colour);
      
      screen_ram_addr += 2;
      colour_ram_addr += 2;
    }
    
    screen_ram_addr += 0x200 - 4*2;  colour_ram_addr += 0x200 - 4*2;
  }
  
  
}

void dialpad_draw(char active_field)
{
  uint8_t seq[12]={1,2,3,
		   4,5,6,
		   7,8,9,
		   11,0,10};

  // Draw GOTOX to right of dialpad, so that right display area
  // remains aligned.
  for(int y=2;y<MAX_ROWS;y++) draw_goto(40,y,40*8-1);

#define X_START 8
  int x = X_START;
  int y = 11;
  for(int d=0;d<=11;d++) {
    // Draw digits all in RED by default
    dialpad_draw_button(seq[d],x,y, (active_field==AF_DIALPAD)? 0x2e : 0x2b);  // 0x20 = reverse
    x+=6;
    if (x>(X_START+6+6)) { x=X_START; y+=5; }
  }

  y=11;
  // Call button : Green unless in a call
  dialpad_draw_button(12,2,y, 0x25);  // 0x20 = reverse
  // Mute button (only valid if not activated and not in a call
  dialpad_draw_button(14,2,y+5, 0x2b);  // 0x20 = reverse
  // Hang up button (only valid if in a call)
  dialpad_draw_button(13,2,y+5+5, 0x2b);  // 0x20 = reverse

  // Draw invisible button to make it all line up
  dialpad_draw_button(13,2,y+5+5+5, 0x06);

}

