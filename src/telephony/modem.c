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

sms_decoded_t sms;

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

int modem_uart_write(uint8_t *buffer, uint16_t size)
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

  }
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
  //  "AT+QSPN", // Show network name  
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
	fprintf(stderr,"DEBUG: AT command '%s' succeeded.\n",modem_init_strings[i]);
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
    if (c=='\r'||c=='\n') {
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
      if (shared.modem_line_len < MODEM_LINE_SIZE) shared.modem_line[shared.modem_line_len++]=c;
    }
    if (counter) counter--;
  }

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

  if (!strncmp((char *)shared.modem_line,"+CMGL",5)) {
    shared.modem_cmgl_counter++;
  }
  if (!strcmp((char *)shared.modem_line,"OK")) shared.modem_saw_ok=1;
  if (!strcmp((char *)shared.modem_line,"ERROR")) shared.modem_saw_error=1;
  
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

    // Clear mute flag when ending a call
    // This call also does the dialpad redraw for us
    modem_unmute_call();
  }
}

void modem_toggle_mute(void)
{
  if (shared.call_state_muted) modem_unmute_call();
  else modem_mute_call();
}

void modem_mute_call(void)
{
  shared.call_state_muted = 1;
  dialpad_draw(shared.active_field, DIALPAD_ALL);
  dialpad_draw_call_state(shared.active_field);    
}

void modem_unmute_call(void)
{
  shared.call_state_muted = 0;
  dialpad_draw(shared.active_field, DIALPAD_ALL);
  dialpad_draw_call_state(shared.active_field);    
}

uint16_t modem_get_sms_count(void)
{
  shared.modem_cmgl_counter=0;
  modem_uart_write((unsigned char *)"AT+CMGL=4\r\n",strlen("AT+CMGL=4\r\n"));
  shared.modem_line_len=0;

  // The EC25 truncates output if more than ~12KB is returned via this command.
  // So we should have a timeout so that it can't hard lock.  

  shared.frame_counter=0;
  
  while(!(shared.modem_saw_ok|shared.modem_saw_error)) {
    if (modem_poll()) {
      fprintf(stderr,"DEBUG: Saw line '%s'\n",shared.modem_line);
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

uint16_t parse_u16_dec(const char *s)
{
    uint16_t v = 0;

    
    while (*s >= '0' && *s <= '9') {
      v = (uint16_t)((v << 3) + (v << 1) + (uint16_t)(*s - '0'));
      s++;
    }
    return v;
}


uint16_t modem_get_oldest_sms(void)
{
  char got_message=0;
  uint16_t sms_number;
  
  shared.modem_cmgl_counter=0;
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

