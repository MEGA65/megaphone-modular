#include "includes.h"

#include "shstate.h"
#include "dialer.h"
#include "screen.h"
#include "records.h"
#include "contacts.h"
#include "buffers.h"
#include "af.h"
#include "modem.h"
#include "smsdecode.h"
#include "format.h"
#include "status.h"

#include <string.h>

sms_decoded_t sms;

//char qltone_string_calling[]="AT+QLTONE=1,400,500,800,30000\r\n";      
char qltone_string_calling[]="AT+QTTS=2,\"ring ring\"\r\n";      
char qltone_string_off[]="AT+QLTONE=0\r\n";      

#ifdef MEGA65
#else

#define _GNU_SOURCE

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <strings.h>
#include <string.h>
#include <ctype.h>
#include <sys/time.h>
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <sys/ioctl.h>
#include <linux/serial.h>
#include <linux/tty_flags.h>
#include <termios.h>
#include <unistd.h>

int fd=-1;

// Dummy declarations for drawing the dial pad or updating the call state display
void dialpad_draw(char active_field,uint8_t button_restrict)
{
}

void dialpad_set_call_state(char call_state)
{
  fprintf(stderr,"INFO: Setting call state to %d\n",call_state);
}

void dialpad_draw_call_state(char active_field)
{
  fprintf(stderr,"INFO: Notifying user of changed call state.\n");
}




void log_error(char *m)
{
  fprintf(stderr,"ERROR: %s\n",m);
}

void set_serial_speed(int fd, int serial_speed)
{
  fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, NULL) | O_NONBLOCK);
  struct termios t;

  if (fd < 0) {
    log_error("set_serial_speed: invalid fd");
    return;
  }

  if (tcgetattr(fd, &t) != 0) { log_error("tcgetattr failed"); return; }  

  if (serial_speed == 115200) {
    if (cfsetospeed(&t, B115200))
      log_error("failed to set output baud rate");
    if (cfsetispeed(&t, B115200))
      log_error("failed to set input baud rate");
  }
  else if (serial_speed == 230400) {
    if (cfsetospeed(&t, B230400))
      log_error("failed to set output baud rate");
    if (cfsetispeed(&t, B230400))
      log_error("failed to set input baud rate");
  }
  else if (serial_speed == 2000000) {
    if (cfsetospeed(&t, B2000000))
      log_error("failed to set output baud rate");
    if (cfsetispeed(&t, B2000000))
      log_error("failed to set input baud rate");
  }
  else if (serial_speed == 1000000) {
    if (cfsetospeed(&t, B1000000))
      log_error("failed to set output baud rate");
    if (cfsetispeed(&t, B1000000))
      log_error("failed to set input baud rate");
  }
  else if (serial_speed == 1500000) {
    if (cfsetospeed(&t, B1500000))
      log_error("failed to set output baud rate");
    if (cfsetispeed(&t, B1500000))
      log_error("failed to set input baud rate");
  }
  else {
    if (cfsetospeed(&t, B4000000))
      log_error("failed to set output baud rate");
    if (cfsetispeed(&t, B4000000))
      log_error("failed to set input baud rate");
  }

  t.c_cflag &= ~PARENB;
  t.c_cflag &= ~CSTOPB;
  t.c_cflag &= ~CSIZE;
  t.c_cflag &= ~CRTSCTS;
  t.c_cflag |= CS8 | CLOCAL;
  t.c_lflag &= ~(ICANON | ISIG | IEXTEN | ECHO | ECHOE);
  t.c_iflag &= ~(BRKINT | ICRNL | IGNBRK | IGNCR | INLCR | INPCK | ISTRIP | IXON | IXOFF | IXANY | PARMRK);
  t.c_oflag &= ~OPOST;
  if (tcsetattr(fd, TCSANOW, &t))
    log_error("failed to set terminal parameters");

  // Also set USB serial port to low latency
  struct serial_struct serial;
  ioctl(fd, TIOCGSERIAL, &serial);
  serial.flags |= ASYNC_LOW_LATENCY;
  ioctl(fd, TIOCSSERIAL, &serial);
  
#ifdef DEBUG
  fprintf(stderr,"DEBUG: Set serial speed and parameters\n");
#endif
}


int open_the_serial_port(char *serial_port,int serial_speed)
{
  if (serial_port == NULL) {
    log_error("serial port not set, aborting");
    return -1;
  }

  errno = 0;
  fd = open(serial_port, O_RDWR | O_NOCTTY);
  if (fd == -1) {
    log_error("could not open serial port");
    return -1;
  }

  set_serial_speed(fd, serial_speed);

  return 0;
}

uint16_t modem_uart_write(uint8_t *buffer, uint16_t size)
{

  uint16_t offset = 0;
  while (offset < size) {
    int written = write(fd, &buffer[offset], size - offset);
    if (written > 0)
      offset += written;
    if (offset < size) {
      usleep(1000);
      fprintf(stderr,"WARN: Wrote %d of %d bytes\n",offset,size);
      perror("write()");
      if (errno!=EAGAIN) exit(-1);
    }
  }
  return size;
}

void print_spaces(FILE *f, int col)
{
  for (int i = 0; i < col; i++)
    fprintf(f, " ");
}

uint16_t modem_uart_read(uint8_t *buffer, uint16_t size)
{
  int count;

  count = read(fd, buffer, size);
  if (count <= 0) return 0;
 
  return count;
}

#endif

char *modem_init_strings[]={
  "ATI", // Make sure modem is alive
  "at+qcfg=\"ims\",1", // Enable VoLTE?  (must be done first in case it reboots the modem)
  "ATE0", // No local echo
  "ATS0=0", // Don't auto-answer
  "ATX4", // More detailed call status indications
  "AT+CMGF=0", // PDU mode for SMS
  // Not supported on the firmware version on this board
  //  "AT+CRC=0", // Say "RING", not "+CRING: ..."
  //  "AT^DCSI=1", // Provide detailed call progression status messages
  //  "AT+CLIP=1", // Present caller ID
  "AT+QINDCFG=\"ring\",1,1", // Enable RING indication
  "AT+QINDCFG=\"ccinfo\",1,1", // Enable RING indication
  "AT+QINDCFG=\"smsincoming\",1,1", // Enable SMS RX indication (+CMTI, +CMT, +CDS)
  "AT+CTZR=2", // Enable network time and timezone indication
  "AT+CSCS=\"GSM\"", // Needed for SMS PDU mode sending?
  "AT+CREG=2", // Enable network registration and status indication
  "AT+CSQ", // Show signal strength
  "AT+QSPN", // Show network name  
  NULL
};

unsigned char modem_happy[6]={'\r','\n','O','K','\r','\n'};
unsigned char modem_sad[6]={'R','R','O','R','\r','\n'};

char modem_init(void)
{
  unsigned char c;
  unsigned char recent[6];
  unsigned char errors=0;

  // Cancel any multi-line input in progress (eg SMS sending)
  unsigned char escape=0x1b;
  modem_uart_write(&escape,1);
  usleep(20000);
  
  // Clear any backlog from the modem
  while (modem_uart_read(&c,1)) continue;

  for(int i=0;modem_init_strings[i];i++) {
    modem_uart_write((unsigned char *)modem_init_strings[i],strlen(modem_init_strings[i]));
    modem_uart_write((unsigned char *)"\r\n",2);

    lfill((unsigned long)recent,0x00,6);
    while(1) {
      unsigned char j;
      if (modem_uart_read(&c,1)) {
	for(j=0;j<(6-1);j++) recent[j]=recent[j+1];
	recent[5]=c;
	// dump_bytes("recent",(unsigned long)recent,6);
      }
      // Check for OK
      for(j=0;j<6;j++) {
	if (recent[j]!=modem_happy[j]) {
	  break;
	}
      }
      if (j==6) {
#ifdef STANDALONE
#ifdef DEBUG
	fprintf(stderr,"DEBUG: AT command '%s' succeeded.\n",modem_init_strings[i]);
#endif
#endif
	break;
      }
      // Check for ERROR
      for(j=0;j<6;j++) {
	if (recent[j]!=modem_sad[j]) {
	  break;
	}
      }
      if (j==6) {
#ifdef STANDALONE
	fprintf(stderr,"DEBUG: AT command '%s' failed.\n",modem_init_strings[i]);
#endif
	errors++;
	break; }
      
    }
    
  }
  return errors;
}

uint8_t enhanced_poll_command_num=0;
char *enhanced_poll_commands[]={
  "AT+CSQ\r\n",  // Get signal strength
  "at+qspn\r\n", // Get network name
  "at+qcfg=\"ims\"\r\n", // Get VoLTE status
  "at+qlts=2\r\n", // Get network time (adjusted to local time)
  "AT+CLCC\r\n", // Query call state
  ""
};

char modem_poll_enhanced(void)
{
  if (shared.modem_line_len==0&&(!shared.modem_response_pending)) {
    // Modem is not doing anything.
    // We can periodically ask the modem to report network name,
    // network time and signal strength
    if ((shared.frame_counter&0x7f)==0x7f) {
      if (!enhanced_poll_commands[enhanced_poll_command_num][0])
	enhanced_poll_command_num=0;
      modem_uart_write((unsigned char *)enhanced_poll_commands[enhanced_poll_command_num],
		       strlen(enhanced_poll_commands[enhanced_poll_command_num]));
      enhanced_poll_command_num++;
    } 
  }
  return modem_poll();
}

char modem_poll(void)
{
  // Check for timeout in state machine
  if ((shared.call_state_timeout != 0)
      && (shared.frame_counter >= shared.call_state_timeout)) {
    shared.call_state_timeout = 0;
    switch(shared.call_state) {
    case CALLSTATE_CONNECTING:
    case CALLSTATE_RINGING:
      modem_hangup_call();    
      break;      
    }
  }
  
  unsigned char c;
  
  // Process upto 255 bytes from the modem
  // (so that we balance efficiency with max run time in modem_poll())
  unsigned char counter=255;

  if (shared.modem_poll_reset_line) {
    shared.modem_line_len=0;
  }
  shared.modem_poll_reset_line = 0;
  
  while (counter&&modem_uart_read(&c,1)) {
    if (c==0x0a||c==0x0d) {
      // End of line
      if (shared.modem_line_len) {
	modem_parse_line();
	// Always return immediately after reading a line, and before
	// we clear the line, so that the caller has an easy way to parse each line
	// of response
	shared.modem_poll_reset_line=1;
	return 1;
      }
      shared.modem_line_len=0;
    } else {
      if (shared.modem_line_len < MODEM_LINE_SIZE)
	shared.modem_line[shared.modem_line_len++]=c;
    }
    if (counter) counter--;
  }

  return 0;
}

char modem_parser_expect_char(char **s, uint8_t expected)
{
  if ((**s)!=expected) return 1;
  (*s)++;
  return 0;
}

char modem_parser_comma(char **s)
{
  return modem_parser_expect_char(s,',');
  if ((**s)!=',') return 1;
  (*s)++;
  return 0;
}

char modem_parser_int16(char **s, uint16_t *out)
{
  char negative=0;
  if ((**s)=='-') { negative=1; (*s)++; }
  uint16_t v = parse_u16_dec(*s);
  while (((**s)>='0')&&((**s)<='9')) (*s)++;
  if (negative) *out=-v; else *out=v;
  return 0;
}

char modem_parser_bcd16(char **s, uint16_t *out)
{
  while (((**s)>='0')&&((**s)<='9')) {
    (*out)<<=4;
    (*out)+=(**s)-'0';
    (*s)++;
  }
  return 0;
}

char modem_parser_quotedstr(char **s, char *out, uint8_t max_len)
{
  uint8_t len=0;
  if ((**s)!='\"') return 1;
  (*s)++;
  while((**s)&&(**s)!='\"') {
    if (len>=(max_len-1)) return -1;
    out[len++]=**s;
    (*s)++;
  }
  out[len]=0;
  if ((**s)!='\"') return 1;
  (*s)++; // skip closing quote
  
  return 0;
}


void modem_parse_line(void)
{
  
  // Check for messages from the modem, and process them accordingly
  // RING
  // CONNECTED
  // NO CARRIER
  // SMS RX notification
  // Network time
  // Network signal
  // Network name
  // Call mute status

  // What else?
  
  if (shared.modem_line_len>= MODEM_LINE_SIZE)
    shared.modem_line_len = MODEM_LINE_SIZE - 1;
  shared.modem_line[shared.modem_line_len]=0;

#ifdef MEGA65
  mega65_uart_print("Modem line: '");
  mega65_uart_print((unsigned char *)shared.modem_line);
  mega65_uart_print("'\r\n");  
#endif


  // '+CSQ: 21,99'
  if (!strncmp((char *)shared.modem_line,"+CSQ: ",6)) {
    char *s = (char *)&shared.modem_line[6];
    uint16_t sig;
    char good=0;
    do {
      if (modem_parser_int16(&s,&sig)) break;
      good=1;
    } while(0);
    if (good) {
      // We add 1 to the level before storing it, so that a value of 0 = not set
      // so that when we launch we don't make the user think there's no signal,
      // rather than we just don't know what signal level.
      shared.signal_level = sig + 1;
      statusbar_draw_signal();
    }
  } 
    // +QLTS: "2026/01/01,16:58:25+42,1"'
    else if (!strncmp((char *)shared.modem_line,"+QLTS: ",7)) {
    char *s = (char *)&shared.modem_line[7];
    uint16_t year,month,day,hour,minute;
    char good=0;
    do {
      // XXX Store numbers as BCD, not ints
      if (modem_parser_expect_char(&s,'\"')) break;
      if (modem_parser_bcd16(&s,&year)) break;
      if (modem_parser_expect_char(&s,'/')) break;
      if (modem_parser_bcd16(&s,&month)) break;
      if (modem_parser_expect_char(&s,'/')) break;
      if (modem_parser_bcd16(&s,&day)) break;
      if (modem_parser_expect_char(&s,',')) break;
      if (modem_parser_bcd16(&s,&hour)) break;
      if (modem_parser_expect_char(&s,':')) break;
      if (modem_parser_bcd16(&s,&minute)) break;
      good=1;
    } while(0);
    if (good) {
      // Store network time and mark it fresh
      shared.nettime_year = year;
      shared.nettime_month = month;
      shared.nettime_day = day;
      shared.nettime_hour = hour;
      shared.nettime_minute = minute;
      shared.nettime_set = 1;
      // (No need to update status bar, as we do that regularly)
      // (But maybe it uses less program RAM to do that here, rather than
      //  having an IRQ doing it?)
    }
  
  }
  // +QSPN: "CHN-UNICOM","UNICOM","",0,"46001"
  else if (!strncmp((char *)shared.modem_line,"+QSPN: ",7)) {
    char *s = (char *)&shared.modem_line[7];
    char network_full[NUMBER_FIELD_LEN+1];
    char network_short[NUMBER_FIELD_LEN+1];
    char good=0;
    do {
      if (modem_parser_quotedstr(&s,network_full,sizeof(network_full))) break;
      if (modem_parser_comma(&s)) break;
      if (modem_parser_quotedstr(&s,network_short,sizeof(network_short))) break;
      if (modem_parser_comma(&s)) break;
      good=1;
    } while(0);
    if (good) {
      strcpy((char *)shared.modem_network_name,network_full);
      statusbar_draw_netname();
    }
  }
  else if (!strncmp((char *)shared.modem_line,"+QCFG: \"ims\",",13)) {
    char *s = (char *)&shared.modem_line[13];
    uint16_t first,second;
    char good=0;
    do {
      if (modem_parser_int16(&s,&first)) break;
      if (modem_parser_comma(&s)) break;
      if (modem_parser_int16(&s,&second)) break;
      good=1;
    } while(0);
    if (good) {
      shared.volte_enabled=second;
      statusbar_draw_volte();
    }
  }  
  else if (!strncmp((char *)shared.modem_line,"+QIND: \"ccinfo\",",16)) {
    char *s = (char *)&shared.modem_line[16];
    uint16_t id,dir,call_state,mode,mpty,number_type;
    char good=0;
    char number[33];
    do {
      if (modem_parser_int16(&s,&id)) break;
      if (modem_parser_comma(&s)) break;
      if (modem_parser_int16(&s,&dir)) break;
      if (modem_parser_comma(&s)) break;
      if (modem_parser_int16(&s,&call_state)) break;
      if (modem_parser_comma(&s)) break;
      if (modem_parser_int16(&s,&mode)) break;
      if (modem_parser_comma(&s)) break;
      if (modem_parser_int16(&s,&mpty)) break;
      if (modem_parser_comma(&s)) break;
      if (modem_parser_quotedstr(&s,number,sizeof(number))) break;
      if (modem_parser_comma(&s)) break;
      if (modem_parser_int16(&s,&number_type)) break;

      good=1;

      uint8_t qltone_mode=0;

      mega65_uart_print("Parsed Call state update.\r\n");

      // Detect if "call 3" (== voice call) has disappeared
      if (id==1&&shared.modem_last_call_id==2) {
	// We have no call state
	if (shared.call_state != CALLSTATE_NUMBER_ENTRY) {
	  if (shared.call_state != CALLSTATE_DISCONNECTED) {
	    shared.call_state = CALLSTATE_DISCONNECTED;

	    // Update display to show call state
	  }
	}
      }
      shared.modem_last_call_id = id;

      // Or if we see state for the current call, then update things
      uint8_t prev_state = shared.call_state;
      // If no caller ID supplied, we can only trust the call_state field
      if (number[0]||call_state) {
	switch(call_state) {
	case 65535: // i.e., -1 : Call terminated
	  shared.call_state = CALLSTATE_DISCONNECTED;
	  break;
	case 0: // CALL ACTIVE
	  shared.call_state = CALLSTATE_CONNECTED;
	  break;
	case 1: // CALL HELD
	  break;
	case 2: // CALLING (outbound)
	  // FALL THROUGH
	case 3: // ALERTING (ringing in handset to indicate call being established) 
	  shared.call_state = CALLSTATE_CONNECTING;
	  qltone_mode='1';
	  break;
	case 4: // RINGING (inbound)
	  shared.call_state = CALLSTATE_RINGING;
	  if (number[0])
	    strncpy((char *)shared.call_state_number,number,NUMBER_FIELD_LEN);
	  else
	    strncpy((char *)shared.call_state_number,"Private (No Caller ID)",NUMBER_FIELD_LEN);
	    
	  shared.call_state_number[NUMBER_FIELD_LEN]=0;
	  break;
	case 5: // WAITING (inbound)
	  // Indicate call waiting? No idea
	  break;
	}
      }
      if (prev_state != shared.call_state) {
	dialpad_draw(shared.active_field, DIALPAD_ALL);
	dialpad_draw_call_state(shared.active_field);
      }
      
      modem_getready_to_issue_command();
      if (qltone_mode)
	modem_uart_write((unsigned char *)qltone_string_calling,strlen(qltone_string_calling));
      else
	modem_uart_write((unsigned char *)qltone_string_off,strlen(qltone_string_off));
      usleep(200000);
      
      
    } while(0);
    if (!good) {
#ifndef MEGA65
      fprintf(stderr,"DEBUG: Failed to parse QIND ccinfo line:\n       '%s'\n",
	      (char *)shared.modem_line);
#endif
    }
  }
  
  
  if (!strncmp((char *)shared.modem_line,"+CMGL",5)) {
    shared.modem_cmgl_counter++;
  }
  if (!strcmp((char *)shared.modem_line,"OK")) {
    shared.modem_saw_ok=1;
    shared.modem_response_pending=0;
  }
  if (!strcmp((char *)shared.modem_line,"ERROR")) {
    shared.modem_saw_error=1;
    shared.modem_response_pending=0;
  }
  
}

char modem_place_call(void)
{
  switch(shared.call_state) {
  case CALLSTATE_DISCONNECTED:
  case CALLSTATE_NUMBER_ENTRY:
    break;
  default:
    return 1;
  }
  
  shared.call_state = CALLSTATE_CONNECTING;
  shared.frame_counter = 0;
  shared.call_state_timeout = MODEM_CALL_ESTABLISHMENT_TIMEOUT_SECONDS * FRAMES_PER_SECOND;

  // Send ATDT to modem
  // The EC25 requires a ; at the end of a number to indicate it's a voice rather
  // than a data call.
  modem_getready_to_issue_command();
  modem_uart_write((unsigned char *)"ATDT",4);
  modem_uart_write((unsigned char *)shared.call_state_number,
		   strlen((char *)shared.call_state_number));
  modem_uart_write((unsigned char *)";\r\n",3); 

  // TTS audio to tell the user that we're dialing
  modem_getready_to_issue_command();
  modem_uart_write((unsigned char *)"AT+QTTS=2,\"dialing\"\r\n",
		   strlen("AT+QTTS=2,\"dialing\"\r\n"));
  
  dialpad_draw(shared.active_field, DIALPAD_ALL);
  dialpad_draw_call_state(shared.active_field);

  return 0;
}

void modem_answer_call(void)
{
  switch (shared.call_state) {
  case CALLSTATE_RINGING:

    shared.call_state = CALLSTATE_CONNECTED;

    // Send ATA to modem
    modem_uart_write((unsigned char *)"ATA\r\n",5); 

    shared.call_state_timeout = 0;

    dialpad_draw(shared.active_field, DIALPAD_ALL);
    dialpad_draw_call_state(shared.active_field);    
  }
}

void modem_hangup_call(void)
{
  // Send ATH0 to modem even if we don't think we're in a call
  modem_getready_to_issue_command();
  modem_uart_write((unsigned char *)"ATH0\r\n",6);
  
  // Clear mute flag when ending a call
  // This call also does the dialpad redraw for us
  modem_unmute_call();

  switch (shared.call_state) {
  case CALLSTATE_CONNECTING:
  case CALLSTATE_RINGING:
  case CALLSTATE_CONNECTED:
    shared.call_state = CALLSTATE_DISCONNECTED;
    shared.call_state_timeout = 0;
  }

  dialpad_draw(shared.active_field, DIALPAD_ALL);
  dialpad_draw_call_state(shared.active_field);
  
}

char modem_set_mic_gain(uint8_t gain)
{
  char num[6];
  num_to_str(gain<<8, num);

  modem_getready_to_issue_command();
  modem_uart_write((unsigned char *)"AT+QMIC=",8);
  modem_uart_write((unsigned char *)num,strlen(num));
  modem_uart_write((unsigned char *)",",1);
  modem_uart_write((unsigned char *)num,strlen(num));
  modem_uart_write((unsigned char *)"\r\n",2);

  return 0;  
}

char modem_set_headset_gain(uint8_t gain)
{
  char num[6];
  num_to_str(gain<<8, num);

  modem_getready_to_issue_command();
  modem_uart_write((unsigned char *)"AT+QRXGAIN=",11);
  modem_uart_write((unsigned char *)num,strlen(num));
  modem_uart_write((unsigned char *)"\r\n",2);

  return 0;    
}

char modem_set_sidetone_gain(uint8_t gain)
{
  char num[6];
  num_to_str(gain<<8, num);
#ifdef DEBUG
  fprintf(stderr,"DEBUG: Sidetone gain = %s\n",num);
#endif
  
  modem_getready_to_issue_command();
  modem_uart_write((unsigned char *)"AT+QSIDET=",10);
  modem_uart_write((unsigned char *)num,strlen(num));
  modem_uart_write((unsigned char *)"\r\n",2);

  return 0;    
}


void modem_toggle_mute(void)
{
  if (shared.call_state_muted) modem_unmute_call();
  else modem_mute_call();
}

void modem_mute_call(void)
{
  shared.call_state_muted = 1;

  modem_getready_to_issue_command();
  modem_uart_write((unsigned char *)"AT+CMUT=1\r\n",11); 
  
  dialpad_draw(shared.active_field, DIALPAD_ALL);
  dialpad_draw_call_state(shared.active_field);    
}

void modem_unmute_call(void)
{
  shared.call_state_muted = 0;

  modem_getready_to_issue_command();
  modem_uart_write((unsigned char *)"AT+CMUT=0\r\n",11); 

  dialpad_draw(shared.active_field, DIALPAD_ALL);
  dialpad_draw_call_state(shared.active_field);    
}

uint16_t modem_get_sms_count(void)
{
  shared.modem_cmgl_counter=0;

  modem_getready_to_issue_command();
  modem_uart_write((unsigned char *)"AT+CMGL=4\r\n",strlen("AT+CMGL=4\r\n"));
  shared.modem_line_len=0;

  // The EC25 truncates output if more than ~12KB is returned via this command.
  // So we should have a timeout so that it can't hard lock.  

  shared.frame_counter=0;
  
  while(!(shared.modem_saw_ok|shared.modem_saw_error)) {
    if (modem_poll()) {
#ifndef MEGA65
      fprintf(stderr,"DEBUG: Saw line '%s'\n",shared.modem_line);
#endif
    }
#ifndef MEGA65
    else {
      // Don't eat all CPU on Linux. Doesn't matter on MEGA65.
      usleep(10000);
      shared.frame_counter++;
    }
#endif
      
    shared.modem_line_len=0;
    // Never wait more than ~3 seconds
    // (Returning too few messages just means a later call after deleting them will
    //  read the rest).
    if (shared.frame_counter > 150 ) break;
  }
  return shared.modem_cmgl_counter;
}

char u16_str[6];
char *u16_to_ascii(uint16_t n)
{
    static const uint16_t divs[5] = { 10000, 1000, 100, 10, 1 };
    int i = 0;
    int started = 0;

    u16_str[0]=0;
    
    if (n == 0) { u16_str[0] = '0'; u16_str[1]=0; return u16_str; }

    for (int k = 0; k < 5; k++) {
        uint16_t d = 0;
        uint16_t base = divs[k];

        while (n >= base) { n -= base; d++; }

        if (started || d || base == 1) {
            u16_str[i++] = (char)('0' + d);
            u16_str[i] = 0;
            started = 1;
        }
    }

    return u16_str;
}

char modem_get_sms(uint16_t sms_number)
{
  // Read the specified SMS message
  modem_getready_to_issue_command();
  modem_uart_write((unsigned char *)"AT+CMGR=",strlen("AT+CMGR="));
  u16_to_ascii(sms_number);
  modem_uart_write((unsigned char*)u16_str,strlen(u16_str));
  modem_uart_write((unsigned char *)"\r\n",2);

  char saw_cmgr=0;
  char got_message=0;

// XXX - Poll and read response
  shared.modem_saw_error = 0;
  shared.modem_saw_ok = 0;
  while(!(shared.modem_saw_ok|shared.modem_saw_error)) {
    if (modem_poll()) {
      if (saw_cmgr) {
	char r = decode_sms_deliver_pdu((char *)shared.modem_line, &sms);
	if (!r)
	  got_message=1;
	else
	  // Found the PDU, but it failed to decode
	  return 100+r;
	saw_cmgr=0;
      }
      else if (!strncmp((char *)shared.modem_line,"+CMGR:",6)) {
	saw_cmgr=1;
      } else saw_cmgr=0;
    }
  }

  if (got_message) return 0; else return 1;
  
}


uint16_t modem_get_oldest_sms(void)
{
  char got_message=0;
  uint16_t sms_number;
  
  shared.modem_cmgl_counter=0;
  modem_getready_to_issue_command();
  modem_uart_write((unsigned char *)"AT+CMGL=4\r\n",strlen("AT+CMGL=4\r\n"));
  shared.modem_line_len=0;
  
  // The EC25 truncates output if more than ~12KB is returned via this command.
  // So we should have a timeout so that it can't hard lock.  
  
  shared.frame_counter=0;
  
  while(!(shared.modem_saw_ok|shared.modem_saw_error)) {
    if (modem_poll()) {
      // Crude way to find the correct lines to decode
      if (shared.modem_cmgl_counter==2) {
	shared.modem_cmgl_counter=3;
	char r = decode_sms_deliver_pdu((char *)shared.modem_line, &sms);
	if (!r)
	  got_message=1;
      }
      if (shared.modem_cmgl_counter==1) {
	shared.modem_cmgl_counter=2;
	sms_number = parse_u16_dec((char *)&shared.modem_line[7]);
      }
    }
#ifndef MEGA65
    else {
    // Don't eat all CPU on Linux. Doesn't matter on MEGA65.
    usleep(10000);
    shared.frame_counter++;
    }
#endif
      
    shared.modem_line_len=0;
    // Never wait more than ~3 seconds
    // (Returning too few messages just means a later call after deleting them will
    //  read the rest).
    if (shared.frame_counter > 150 ) break;
  }
  if (got_message) return sms_number;
  else return 0xffff;
}


char modem_delete_sms(uint16_t sms_number)
{
  // Delete the specified SMS number
  modem_getready_to_issue_command();
  modem_uart_write((unsigned char *)"AT+CMGD=",strlen("AT+CMGD="));
  u16_to_ascii(sms_number);
  modem_uart_write((unsigned char*)u16_str,strlen(u16_str));
  modem_uart_write((unsigned char *)"\r\n",2);

  shared.modem_saw_error = 0;
  shared.modem_saw_ok = 0;
  shared.modem_line_len=0;
  while(!(shared.modem_saw_ok|shared.modem_saw_error)) {
    modem_poll();
    shared.modem_line_len=0;
  }
  
  return shared.modem_saw_error;
}

void modem_query_volte(void)
{
  modem_getready_to_issue_command();
  modem_uart_write((unsigned char *)"at+qcfg=\"ims\"\r\n",15);
}

void modem_query_network(void)
{
  modem_getready_to_issue_command();
  modem_uart_write((unsigned char *)"at+qspn\r\n",9);
}

void modem_getready_to_issue_command(void)
{
  while (shared.modem_response_pending) {
    usleep(1000);
    shared.modem_response_pending--;
    modem_poll();
  }
  shared.modem_response_pending=1000;
}

#ifdef STANDALONE
int main(int argc,char **argv)
{
  if (argc<3) {
    fprintf(stderr,"usage: powerctl <serial port> <serial speed> [command [...]]\n");
    exit(-1);
  }

  open_the_serial_port(argv[1],atoi(argv[2]));

  for(int i=3;i<argc;i++) {
    if (!strcmp(argv[i],"init")) {
      modem_init();
    }
    else if (!strcmp(argv[i],"smscount")) {
      uint16_t sms_count = modem_get_sms_count();
      fprintf(stderr,"INFO: %d SMS messages on SIM card.\n",sms_count);      
    }
else if (!strncmp(argv[i], "smssend=", 8)) {
        /* Format: smssend=NUMBER,MESSAGE */
        char *p = argv[i] + 8;
        char *comma = strchr(p, ',');
        
        if (comma) {
            /* Static allocation to keep stack frame small on 6502 */
            static char recipient[32]; 
            static uint8_t msg_ref = 1; /* Persists across calls */

	    if (!msg_ref) msg_ref++;
	    
            /* calculate length of number part */
            size_t num_len = comma - p;
            
            if (num_len < sizeof(recipient)) {
                memcpy(recipient, p, num_len);
                recipient[num_len] = '\0'; /* Null-terminate the number */
                
                char *msg_body = comma + 1; /* Message starts after comma */
                
                printf("Sending SMS to %s (%d chars)...\n", recipient, (int)strlen(msg_body));
                
                /* Call the encoder */
                sms_send_utf8(recipient, msg_body, msg_ref++);
                
            } else {
                fprintf(stderr, "Error: Recipient number too long.\n");
            }
        } else {
            fprintf(stderr, "Usage: smssend=NUMBER,MESSAGE\n");
        }
    }
    else if (!strncmp(argv[i],"smsdel=",7)) {
      uint16_t sms_number = atoi(&argv[i][7]);      
      char result = modem_delete_sms(sms_number);
      if (result) {
	fprintf(stderr,"ERROR: modem_delete_sms() returned %d\n",result);
      }
    }
    else if (!strncmp(argv[i],"smsget=",7)) {
      uint16_t sms_number = atoi(&argv[i][7]);
      char result = modem_get_sms(sms_number);
      if (!result) {
	fprintf(stderr,"INFO: Decoded SMS message:\n");
	fprintf(stderr,"       Sender: %s\n",sms.sender);
	fprintf(stderr,"    Send time: %04d/%02d/%02d %02d:%02d.%02d (TZ%+dmin)\n",
		sms.year, sms.month, sms.day,
		sms.hour, sms.minute, sms.second,
		sms.tz_minutes);
	fprintf(stderr,"       text: %s\n",sms.text);
	fprintf(stderr,"       concat: %d\n",sms.concat);
	fprintf(stderr,"       concat_ref: %d\n",sms.concat_ref);
	fprintf(stderr,"       concat_total: %d\n",sms.concat_total);
	fprintf(stderr,"       concat_seq: %d\n",sms.concat_seq);

      } else
	fprintf(stderr,"ERROR: Could not retreive or decode SMS message.\n");
    }
    else if (!strcmp(argv[i],"smsnext")) {
      uint16_t result = modem_get_oldest_sms();
      if (result!=0xffff) {
	fprintf(stderr,"INFO: Decoded SMS message #%d:\n", result);
	fprintf(stderr,"       Sender: %s\n",sms.sender);
	fprintf(stderr,"    Send time: %04d/%02d/%02d %02d:%02d.%02d (TZ%+dmin)\n",
		sms.year, sms.month, sms.day,
		sms.hour, sms.minute, sms.second,
		sms.tz_minutes);
	fprintf(stderr,"       text: %s\n",sms.text);
	fprintf(stderr,"       concat: %d\n",sms.concat);
	fprintf(stderr,"       concat_ref: %d\n",sms.concat_ref);
	fprintf(stderr,"       concat_total: %d\n",sms.concat_total);
	fprintf(stderr,"       concat_seq: %d\n",sms.concat_seq);

      } else
	fprintf(stderr,"ERROR: Could not retreive or decode SMS message.\n");
    }
    else if (!strncmp(argv[i],"headset=",8)) {
      fprintf(stderr,"INFO: Setting headset level to '%s'\n",&argv[i][8]);
      modem_set_headset_gain(atoi(&argv[i][8])*2.55);
    }
    else if (!strncmp(argv[i],"micgain=",7)) {
      fprintf(stderr,"INFO: Setting mic gain level to '%s'\n",&argv[i][7]);
      modem_set_mic_gain(atoi(&argv[i][7])*2.55);
    }
    else if (!strncmp(argv[i],"sidetone=",9)) {
      fprintf(stderr,"INFO: Setting side-tone level to '%s'\n",&argv[i][9]);
      modem_set_sidetone_gain(atoi(&argv[i][9])*2.55);
    }
    else if (!strncmp(argv[i],"callrx",6)) {
      while(shared.call_state != CALLSTATE_DISCONNECTED) {
	modem_poll();
	if (shared.call_state == CALLSTATE_RINGING) {
	  fprintf(stderr,"Answer incoming call from '%s'\n",
		  shared.call_state_number);
	  modem_answer_call();
	}
      }
    }
    else if (!strncmp(argv[i],"volte",5)) {
      shared.volte_enabled=99;
      modem_query_volte();
      while(shared.volte_enabled==99) modem_poll();
      fprintf(stderr,"INFO: VoLTE is%s enabled.\n",
	      shared.volte_enabled?"":" not");
    }
    else if (!strncmp(argv[i],"network",7)) {
      shared.modem_network_name[0]=0;
      modem_query_network();
      while(!shared.modem_network_name[0]) modem_poll();
      fprintf(stderr,"INFO: Network name is '%s'.\n",
	      shared.modem_network_name);      
    }
    else if (!strncmp(argv[i],"call=",5)) {
      fprintf(stderr,"INFO: Dialing '%s'\n",&argv[i][5]);
      if (strlen(&argv[i][5])>=NUMBER_FIELD_LEN) {
	fprintf(stderr,"ERROR: Requested number is too long.\n");
	exit(-1);
      }
      strcpy((char *)shared.call_state_number,&argv[i][5]);
      modem_place_call();

      fprintf(stderr,"INFO: Monitoring modem until call concludes.\n");
      while(shared.call_state != CALLSTATE_DISCONNECTED
	    && shared.call_state != CALLSTATE_NUMBER_ENTRY) {
	modem_poll();
      }
      fprintf(stderr,"INFO: Call ended in state %s\n",
	      shared.call_state==CALLSTATE_DISCONNECTED?"DISCONNECTED":"NUMBER ENTRY");
      
    }
    else if (!strcmp(argv[i],"hangup"))
      modem_hangup_call();
    
  }
}
#endif
