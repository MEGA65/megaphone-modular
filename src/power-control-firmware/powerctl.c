#ifdef MEGA65
#else

#define _GNU_SOURCE

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
  
  fprintf(stderr,"DEBUG: Set serial speed and parameters\n");
}


int open_the_serial_port(char *serial_port,int serial_speed)
{
  if (serial_port == NULL) {
    log_error("serial port not set, aborting");
    return -1;
  }

  errno = 0;
  fd = open(serial_port, O_RDWR);
  if (fd == -1) {
    log_error("could not open serial port");
    return -1;
  }

  set_serial_speed(fd, serial_speed);


  return 0;
}

int do_serial_port_write(uint8_t *buffer, size_t size)
{

  size_t offset = 0;
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

int dump_bytes(int col, char *msg, unsigned char *bytes, int length)
{
  print_spaces(stderr, col);
  fprintf(stderr, "%s:\n", msg);
  for (int i = 0; i < length; i += 16) {
    print_spaces(stderr, col);
    fprintf(stderr, "%04X: ", i);
    for (int j = 0; j < 16; j++)
      if (i + j < length)
        fprintf(stderr, " %02X", bytes[i + j]);
      else
        fprintf(stderr, " | ");
    fprintf(stderr, "  ");
    for (int j = 0; j < 16; j++)
      if (i + j < length)
        fprintf(stderr, "%c", (bytes[i + j] >= ' ' && bytes[i + j] < 0x7c) ? bytes[i + j] : '.');

    fprintf(stderr, "\n");
  }
  return 0;
}


size_t do_serial_port_read(uint8_t *buffer, size_t size)
{
  int count;

  count = read(fd, buffer, size);

  return count;
}

void do_serial_port_flush(void)
{
  uint8_t buf[16];
  while(do_serial_port_read(buf,16)>0) continue;
}

#endif

char line_buffer[128];
uint8_t line_len=0;

char powerctl_read_line(void)
{
  line_len=0;
  uint8_t buf=1;
  while(1) {
    if (do_serial_port_read(&buf,1)==1) {
      if (!buf) return 0xff;
      // 0x80 -- 0xff status bytes should not occur.
      // if we see one, it's because we're not dumping config anymore = return failure
      if (buf&0x80) return 0;
      line_buffer[line_len++]=buf;
      if (buf==0x0d || buf==0x0a) {
	line_buffer[line_len]=0;
	//	dump_bytes(0,"Read line",line_buffer,line_len);
	return line_len;
      }
    } else usleep(1000);
  }
}

char powerctl_versioncheck(uint8_t *major, uint8_t *minor)
{
  char saw_major=0;
  while(1) {
    if (!powerctl_read_line()) return 0xff;
    if (!strncmp(line_buffer,"VER:",4)) {
      if (major) *major = line_buffer[4]-'0';
      // Unsupported version
      if (line_buffer[4]!='1') return 0xff;
      saw_major=1;
    }
    if (!strncmp(line_buffer,"MINOR:",6)) {
      if (minor) *minor = line_buffer[6]-'0';
      // Success (assuming we saw the major version number already)
      if (saw_major) return 0; else return 0xff;
    }
  }
  // Could not find major or minor version
  return 0xff;
}

char powerctl_start_read_config(void)
{ 
  // Clear any pending input
  do_serial_port_flush();
  // Send command to retrieve config
  do_serial_port_write((unsigned char *)"?",1);

  if (powerctl_versioncheck(NULL,NULL)) return 0xff;

  return 0;
}


  
char powerctl_get_circuit_count(void)
{
  if (powerctl_start_read_config()) return 0;

  while(1) {
    if (!powerctl_read_line()) return 0;
    if (!strncmp(line_buffer,"CIRCUITS:",9)) {
      return line_buffer[9]-'0';
    }
  }
  
  return 0;
}

char powerctl_getconfig(char circuit_id,char field_id,unsigned char *out,uint8_t out_len)
{
  powerctl_start_read_config();

  while(!powerctl_read_line()) {
    if (line_buffer[1]==':' && line_buffer[0]==(circuit_id+'0')) {
    }
  }

  return 0xff;  
}

int main(int argc,char **argv)
{
  if (argc<3) {
    fprintf(stderr,"usage: powerctl <serial port> <serial speed> [command [...]]\n");
    exit(-1);
  }

  open_the_serial_port(argv[1],atoi(argv[2]));

  for(int i=3;i<argc;i++) {
    if (!strcmp(argv[i],"config")) {
      // Do a first pass to get circuit count
      char circuit_count = powerctl_get_circuit_count();
      if (!circuit_count) {
	fprintf(stderr,"ERROR: Could not read count of controlled circuits\n");
	exit(-1);
      }
      fprintf(stderr,"INFO: System controls %d circuits\n",circuit_count);
      for(int circuit_id=0;circuit_id<circuit_count;circuit_id++) {
	// Stop when we fail to read info for a circuit
	if (powerctl_getconfig(circuit_id,1,NULL,0)) break;
      }
    }
  }
  
}
