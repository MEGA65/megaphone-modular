#include "includes.h"

#include "shstate.h"
#include "dialer.h"
#include "screen.h"
#include "records.h"
#include "contacts.h"
#include "buffers.h"
#include "af.h"
#include "modem.h"

// XXX Use the fact that chip RAM at 0x60000 reads as zeroes :)
#define DIALPAD_BLANK_GLYPH_ADDR 0x60000

static unsigned char MSG_DIAL[]
= "Dial number or F1 to dial contact";
static unsigned char MSG_CALLING[]
= "Calling...";
static unsigned char MSG_INCOMING[]
= "Incoming Call...";
static unsigned char MSG_EMPTY[]
= "";
static unsigned char MSG_INCALL[]
= "In Call ";
static unsigned char MSG_ENDED[]
= "Call Ended.";

unsigned char *call_state_messages[CALLSTATE_MAX+1]={
  MSG_DIAL,
  MSG_CALLING,
  MSG_INCOMING,
  MSG_INCALL,
  MSG_ENDED
};

#define CALL_STATE_LINES 6
#define CALL_STATE_LINE_DIALED_NUMBER 3
#define CALL_STATE_LINE_DTMF_HISTORY 5
uint8_t call_state_colours[CALL_STATE_LINES]={
  0x01,    // Instructions
  0x06,    // Blank line
  0x0e,    // Contact name
  0x81,    // Number being dialed
  0x06,    // Blank line
  0x81     // DTMF dial history
};

unsigned char dialpad_lookup_button(unsigned char d)
{
  if (d>='1'&&d<='9') return d-'1';
  if (d=='*') return 9;
  if (d=='0') return 10;
  if (d=='#') return 11;
  return 99;
}

void dialpad_draw_call_state(char active_field)
{
  if (shared.call_state>CALLSTATE_MAX)
    shared.call_state = CALLSTATE_NUMBER_ENTRY;

  unsigned char *s;
  
  // Neither number field is active by default
  call_state_colours[CALL_STATE_LINE_DIALED_NUMBER]=0x8c;
  call_state_colours[CALL_STATE_LINE_DTMF_HISTORY]=0x06;

  switch (shared.call_state) {
  case CALLSTATE_CONNECTED:
  case CALLSTATE_DISCONNECTED:
    call_state_colours[CALL_STATE_LINE_DTMF_HISTORY]=0x86;
  }
  
  if (active_field==AF_DIALPAD) {
    if (shared.call_state == CALLSTATE_CONNECTED)
      call_state_colours[CALL_STATE_LINE_DTMF_HISTORY]=0x81;
    else   
      call_state_colours[CALL_STATE_LINE_DIALED_NUMBER]=0x81;
  }
  
  for(uint8_t l=0;l<CALL_STATE_LINES;l++) {
    s = MSG_EMPTY;
    switch(l) {
    case 0: s = call_state_messages[(uint8_t)shared.call_state]; break;
    case 2: s = shared.call_state_contact_name; break;
    case 3: s = shared.call_state_number; break;
    case 5: s = shared.call_state_dtmf_history; break;
    }
    //    mega65_uart_printptr(s);
    draw_string_nowrap(2,3+l,
		       FONT_UI,
		       call_state_colours[l],
		       s,
		       16,
		       RIGHT_AREA_START_PX - 16 - 40,
		       RIGHT_AREA_START_GL - 2,
		       NULL,
		       VIEWPORT_PADDED_RIGHT,
		       NULL,
		       NULL);
  }

  
}
  
void dialpad_set_call_state(char new_state)
{

  if (shared.call_state>CALLSTATE_MAX)
    shared.call_state = CALLSTATE_NUMBER_ENTRY;
  
  
  switch(new_state) {
  case CALLSTATE_NUMBER_ENTRY:
    break;
  case CALLSTATE_CONNECTING:
    break;
  case CALLSTATE_CONNECTED:
    if (shared.call_state == CALLSTATE_RINGING) {
      // XXX Log accepted inbound call call  "➡️☎️ at XYZ"
    }
    if (shared.call_state == CALLSTATE_CONNECTING) {      
      // XXX Log successful outbound call "☎️➡️ at XYZ"
    }
    break;
  case CALLSTATE_DISCONNECTED:
    switch(shared.call_state) {
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
    switch (shared.call_state) {
    case CALLSTATE_NUMBER_ENTRY:
      // Erase contact info
      shared.call_contact_id = -1;
      shared.call_state_contact_name[0]=0;
      shared.call_state_number[0]=0;
      shared.call_state_dtmf_history[0]=0;
    }
    break;
  }

  shared.call_state = new_state;		     
  
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

void dialpad_draw(char active_field, uint8_t button_restrict)
{
  uint8_t seq[12]={1,2,3,
		   4,5,6,
		   7,8,9,
		   11,0,10};

  // Draw GOTOX to right of dialpad, so that right display area
  // remains aligned.
  // Note that we use y=3 as start, so that if we skip any pixels the blank line 2
  // will be used as the source.
  for(int y=3;y<MAX_ROWS;y++) draw_goto(RIGHT_AREA_START_GL-1,y,RIGHT_AREA_START_PX);

#define X_START 8
  int x = X_START;
  int y = 11;
  uint8_t colour = (active_field==AF_DIALPAD)? 0x2e : 0x2b;  // 0x20 = reverse
  if (button_restrict!=DIALPAD_ALL) colour = 0x22; // red highlight
  for(int d=0;d<=11;d++) {
    // Draw digits all in RED by default
    if (d==button_restrict||(button_restrict==DIALPAD_ALL))
      dialpad_draw_button(seq[d],x,y, colour);
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

unsigned char *dialpad_current_string(void)
{
  if (shared.call_state==CALLSTATE_CONNECTED) return shared.call_state_dtmf_history;
  return shared.call_state_number;
}
  


void dialpad_dial_digit(unsigned char d)
{
  if (d>'a'&&d<='d') d=d-'a'+'A';
  if ((d>='0'&&d<='9')||(d>='A'&&d<='D')||(d=='*')||(d=='#')||(d=='+')||(d==0x14)) {
    // Is valid DTMF or other dialing character

    // Erase contact name if we are hand-modifying the number being dialed
    switch (shared.call_state) {
    case CALLSTATE_NUMBER_ENTRY:
    case CALLSTATE_DISCONNECTED:    
      shared.call_state = CALLSTATE_NUMBER_ENTRY;
      shared.call_state_contact_name[0]=0;
    }
    
    unsigned char *s = dialpad_current_string();

    uint8_t button_id = dialpad_lookup_button(d);

    // Highlight button
    if (button_id!=99) {
      dialpad_draw(AF_DIALPAD,button_id);
      // Wait a few frames to make highlight obvious
      shared.frame_counter = 0x00;
      while(shared.frame_counter<0x05) continue;
    }
    
    for(uint8_t o=0;o<NUMBER_FIELD_LEN;o++) {
      if ((!s[o])||(s[o]==CURSOR_CHAR)) {
	s[o]=d;
	if (d==0x14) {
	  if (o>0) { s[o-1]=CURSOR_CHAR; s[o]=0; }
	  else { s[0]=CURSOR_CHAR; s[1]=0; }
	} else 	{
	  s[o+1]=CURSOR_CHAR;
	  s[o+2]=0;
	}
	break;
      }
    }
    s[NUMBER_FIELD_LEN-1]=0;

    // Erase any contact name we are displaying
    
    
    dialpad_draw_call_state(AF_DIALPAD);

    // Clear highlight on button
    dialpad_draw(AF_DIALPAD,DIALPAD_ALL);
  }

  
}

void dialpad_clear(void)
{
    unsigned char *s = dialpad_current_string();
    s[0]=CURSOR_CHAR;
    s[1]=0;
    dialpad_draw_call_state(AF_DIALPAD);
}

void dialpad_hide_show_cursor(char active_field)
{
  unsigned char *s = dialpad_current_string();
  
  for(uint8_t o=0;o<NUMBER_FIELD_LEN;o++) {
    if ((!s[o])||(s[o]==CURSOR_CHAR)) {
      if (active_field==AF_DIALPAD) {
	s[o]=CURSOR_CHAR;
	s[o+1]=0;
      } else s[o]=0;
      break;
    }
  }
  s[NUMBER_FIELD_LEN-1]=0;  
}

void dialer_dial_contact(void)
{
  switch (shared.call_state) 
    {
    case CALLSTATE_NUMBER_ENTRY:
    case CALLSTATE_DISCONNECTED:
      // Load contact
      if (!contact_read(shared.contact_id,buffers.textbox.contact_record)) {      
	// Copy number to the dialer
	unsigned char *s = dialpad_current_string();
	unsigned char *phoneNumber
	  = find_field(buffers.textbox.contact_record, RECORD_DATA_SIZE,
		       FIELD_PHONENUMBER,NULL);    
	unsigned char *firstName
	  = find_field(buffers.textbox.contact_record, RECORD_DATA_SIZE,
		       FIELD_FIRSTNAME,NULL);    
	unsigned char *lastName
	  = find_field(buffers.textbox.contact_record, RECORD_DATA_SIZE,
		       FIELD_LASTNAME,NULL);    

	// Set contact name for dialed number
	// (We do this before the numbers match check, so that if you
	// hand-dial a contact, and then hit F1 on that contact, it will
	// still draw the contact name as well as immediately place the call.
	{
	  unsigned char o_ofs=0;
	  unsigned char i_ofs=0;
	  for(i_ofs=0;firstName[i_ofs];i_ofs++) {
	    if (o_ofs<=NUMBER_FIELD_LEN) 
	      shared.call_state_contact_name[o_ofs++] = firstName[i_ofs];
	  }
	  if (o_ofs) shared.call_state_contact_name[o_ofs++] = ' ';
	  for(i_ofs=0;lastName[i_ofs];i_ofs++) {
	    if (o_ofs<=NUMBER_FIELD_LEN) 
	      shared.call_state_contact_name[o_ofs++] = lastName[i_ofs];
	  }
	  shared.call_state_contact_name[o_ofs++] = 0;
	}	  

	uint8_t numbersMatch = 1;
	if (phoneNumber&&phoneNumber[0]&&s[0]&&(s[0]!=CURSOR_CHAR)) {
	  for(uint8_t i=0;s[i];i++)
	    if ((s[i]!=CURSOR_CHAR)&&(phoneNumber[i]!=s[i])) { numbersMatch=0; break; }
	} else numbersMatch=0;
	if (numbersMatch) {
	  // We already have this number loaded, so actually trigger the call.
	  modem_place_call();
	} else {
	  lcopy((unsigned long)phoneNumber,
		(unsigned long)s,
		NUMBER_FIELD_LEN);
	  // A couple of extra bytes are allocated to make sure it fits.
	  s[NUMBER_FIELD_LEN]=0;
	  
	  for(uint8_t i=0;i<=NUMBER_FIELD_LEN;i++) {
	    if(!s[i]||s[i]==CURSOR_CHAR) {
	      s[i]=CURSOR_CHAR;
	      s[i+1]=0;
	      break;
	    }
	  }
	  
	  if (shared.call_state == CALLSTATE_DISCONNECTED) {
	    shared.call_state = CALLSTATE_NUMBER_ENTRY;
	  }
	  
	  // Cause dialpad to be redrawn
	  dialpad_draw_call_state(shared.active_field);
	}
      }
      break;
    }

}
