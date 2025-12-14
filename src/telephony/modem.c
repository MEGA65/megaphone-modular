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

int celmodem_uart_write(uint8_t *buffer, uint16_t size)
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

uint16_t celmodem_uart_read(uint8_t *buffer, uint16_t size)
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
}
#endif


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



