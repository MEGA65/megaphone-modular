#include "includes.h"

#include "shstate.h"
#include "dialer.h"
#include "screen.h"
#include "records.h"
#include "contacts.h"
#include "buffers.h"
#include "af.h"
#include "modem.h"

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

  fprintf(stderr,"DEBUG: shared=%p\n",&shared);
  fflush(stderr);
  
  for(int i=3;i<argc;i++) {
    if (!strcmp(argv[i],"init")) {
      modem_init();
    }
    else if (!strcmp(argv[i],"smscount")) {
      modem_init();
      uint16_t sms_count = modem_get_sms_count();
      fprintf(stderr,"INFO: %d SMS messages on SIM card.\n",sms_count);      
    }
  }
}
#endif

char *modem_init_strings[]={
  "ATI", // Make sure modem is alive
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
  "AT+CREG=2", // Enable network registration and status indication
  "at+qcfg=\"ims\",1" // Enable VoLTE?  
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


void modem_poll(void)
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
  while (counter&&modem_uart_read(&c,1)) {
    if (c=='\r'||c=='\n') {
      // End of line
      if (shared.modem_line_len) modem_parse_line();
      shared.modem_line_len=0;
    } else {
      if (shared.modem_line_len < MODEM_LINE_SIZE) shared.modem_line[shared.modem_line_len++]=c;
    }
    if (counter) counter--;
  }
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
  shared.modem_cgml_counter=0;
  modem_uart_write((unsigned char *)"AT+CMGL=4\r\n",strlen("AT+CMGL=4\r\n"));
  while(!(shared.modem_saw_ok|shared.modem_saw_error)) {
    modem_poll();
#ifndef MEGA65
    // Don't eat all CPU on Linux. Doesn't matter on MEGA65.
    usleep(10);
#endif
  }
  return shared.modem_cgml_counter;
}
