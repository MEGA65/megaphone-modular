#include "includes.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "uart.h"

void print_box(unsigned char x1, unsigned char y1,
	       unsigned char x2, unsigned char y2,
	       unsigned char colour)
{
  uint16_t char_addr = 0xf000 + y1 * 80;
  for(int x=x1;x<=x2;x++) {
    POKE(char_addr+x,0x20);
    lpoke(0xff80000L - 0xf000 + char_addr+x, 0x20 | colour);    
  }
  for(int y=y1+1;y<y2;y++) {
    char_addr+=80;
    POKE(char_addr+x1,0x20);
    lpoke(0xff80000L - 0xf000 + char_addr+x1, 0x20 | colour);    
    POKE(char_addr+x2,0x20);
    lpoke(0xff80000L - 0xf000 + char_addr+x2, 0x20 | colour);        
  }
  char_addr+=80;
  for(int x=x1;x<=x2;x++) {
    POKE(char_addr+x,0x20);
    lpoke(0xff80000L - 0xf000 + char_addr+x, 0x20 | colour);    
  }
}

void print_text80(unsigned char x, unsigned char y, unsigned char colour, char *msg)
{
  uint16_t char_addr = 0xf000 + x + y * 80;
  while (*msg) {
    uint8_t char_code = *msg;
    POKE(char_addr + 0, char_code);
    lpoke(0xff80000L - 0xf000 + char_addr, colour);
    msg++;
    char_addr += 1;
  }
}

#include "ascii-font.c"

void h640_text_mode(void)
{
  POKE(0xD018, 0x16);
  POKE(0xD054, 0x00);
  POKE(0xD031, 0xE8);
  POKE(0xD016, 0xC9);
  POKE(0xD058, 80);
  POKE(0xD059, 80 / 256);
  POKE(0xD05E, 80);
  POKE(0xD060, 0x00);
  POKE(0xD061, 0xf0);
  POKE(0xD062, 0x00);
  POKE(0xD07B, 50);

  lcopy((uint16_t)&ascii_font[0],0xe000,4096);
  POKE(0xD068,0x00);
  POKE(0xD069,0xE0);
  
  lfill(0xf000, 0x20, 4000);
  lfill(0xff80000L, 0x0E, 4000);
}

void send_cmd(char *cmd) {
  modem_uart_write((uint8_t*)cmd, strlen(cmd));
  uint8_t cr = '\r';
  modem_uart_write(&cr, 1);
}

void sleep_approx_1s() {
  uint8_t last_raster = PEEK(0xD012);
  int frames = 0;
  while (frames < 50) {
    uint8_t current_raster = PEEK(0xD012);
    if (current_raster < last_raster) frames++;
    last_raster = current_raster;
  }
}

int dbg_x = 0;
int dbg_y = 2;

const char hex_chars[] = "0123456789ABCDEF";

void debug_char(uint8_t c) {
  if (c >= 32 && c <= 126) {
    POKE(0xf000 + dbg_y * 80 + 40 + dbg_x, c);
    lpoke(0xff80000L - 0xf000 + (0xf000 + dbg_y * 80 + 40 + dbg_x), 0x05);
  } else {
    char hex[4];
    hex[0] = '\\';
    hex[1] = hex_chars[c >> 4];
    hex[2] = hex_chars[c & 0x0F];
    hex[3] = 0;
    
    for(int j=0; hex[j]; j++) {
       POKE(0xf000 + dbg_y * 80 + 40 + dbg_x, hex[j]);
       lpoke(0xff80000L - 0xf000 + (0xf000 + dbg_y * 80 + 40 + dbg_x), 0x08);
       dbg_x++;
       if (dbg_x >= 38) { dbg_x = 0; dbg_y++; }
    }
    dbg_x--; 
  }
  dbg_x++;
  if (dbg_x >= 38) { dbg_x = 0; dbg_y++; }
  if (dbg_y >= 48) { 
    dbg_y = 2; 
    dbg_x = 0; 
    for(int i=2; i<48; i++) {
      lfill(0xf000 + i * 80 + 40, 0x20, 38); 
      lfill(0xff80000L + i * 80 + 40, 0x0e, 38);
    }
  }
}

void wait_for_modem() {
  char buf[80];
  int buf_idx = 0;
  
  while (1) {
    print_text80(2, 3, 0x0a, "Waiting for JTAG modem to respond... ");
    send_cmd("AT");
    
    uint32_t frames_passed = 0;
    uint8_t last_raster = PEEK(0xD012);
    int got_ok = 0;

    // wait up to 100 frames (2 seconds)
    while(frames_passed < 100) {
      uint8_t current_raster = PEEK(0xD012);
      if (current_raster < last_raster) frames_passed++;
      last_raster = current_raster;

      uint8_t rx_buf[64];
      uint16_t count = modem_uart_read(rx_buf, sizeof(rx_buf));
      if (count > 0) {
        frames_passed = 0; // reset timeout on receiving character
        for (uint16_t i=0; i<count; i++) {
          uint8_t c = rx_buf[i];
          debug_char(c);
          if (c == '\r' || c == '\n') {
            if (buf_idx > 0) {
              buf[buf_idx] = 0;
              if (strncmp(buf, "OK", 2) == 0) {
                got_ok = 1;
              }
              buf_idx = 0;
            }
          } else {
            if (buf_idx < 79) buf[buf_idx++] = c;
          }
        }
      }
      if (got_ok) break;
    }
    if (got_ok) {
      print_text80(2, 3, 0x05, "Modem OK!                            ");
      break;
    }
    sleep_approx_1s();
  }
}

char ati_version[80] = "";
char ati_build[80] = "";
char ati_identity[80] = "";
char ati_sdcard[80] = "";
char ati_wifi_hw[80] = "";

char wifi_url[80] = "";

void query_info() {
  print_text80(2, 4, 0x07, "Querying ATI...                      ");
  send_cmd("ATI");
  
  char buf[128];
  int buf_idx = 0;
  int line_count = 0;
  
  uint32_t frames_passed = 0;
  uint8_t last_raster = PEEK(0xD012);
  int done = 0;
  
  // Wait up to 5 seconds
  while(frames_passed < 250 && !done) {
    uint8_t current_raster = PEEK(0xD012);
    if (current_raster < last_raster) frames_passed++;
    last_raster = current_raster;

    uint8_t rx_buf[64];
    uint16_t count = modem_uart_read(rx_buf, sizeof(rx_buf));
    if (count > 0) {
      frames_passed = 0;
      for (uint16_t i=0; i<count; i++) {
        uint8_t c = rx_buf[i];
        debug_char(c);
        if (c == '\r' || c == '\n') {
          if (buf_idx > 0) {
            buf[buf_idx] = 0;
            if (strncmp(buf, "OK", 2) == 0 || strncmp(buf, "ERROR", 5) == 0) {
              done = 1;
            }
            if (strncmp(buf, "ATI", 3) != 0 && !done) {
              if (line_count == 0) {
                strncpy(ati_version, buf, 79);
              } else if (strncmp(buf, "BUILD:", 6) == 0) {
                strncpy(ati_build, buf, 79);
              } else if (strncmp(buf, "IDENTITY:", 9) == 0) {
                strncpy(ati_identity, buf, 79);
              } else if (strncmp(buf, "SDCARD:", 7) == 0) {
                strncpy(ati_sdcard, buf, 79);
              } else if (strncmp(buf, "WIFI:", 5) == 0) {
                strncpy(ati_wifi_hw, buf, 79);
              }
              line_count++;
            }
            buf_idx = 0;
          }
        } else {
          if (buf_idx < 127) buf[buf_idx++] = c;
        }
      }
    }
  }

  print_text80(2, 4, 0x07, "Querying AT+WIFI?...                 ");
  send_cmd("AT+WIFI?");
  buf_idx = 0;
  frames_passed = 0;
  last_raster = PEEK(0xD012);
  done = 0;

  while(frames_passed < 250 && !done) {
    uint8_t current_raster = PEEK(0xD012);
    if (current_raster < last_raster) frames_passed++;
    last_raster = current_raster;

    uint8_t rx_buf[64];
    uint16_t count = modem_uart_read(rx_buf, sizeof(rx_buf));
    if (count > 0) {
      frames_passed = 0;
      for (uint16_t i=0; i<count; i++) {
        uint8_t c = rx_buf[i];
        debug_char(c);
        if (c == '\r' || c == '\n') {
          if (buf_idx > 0) {
            buf[buf_idx] = 0;
            if (strncmp(buf, "OK", 2) == 0 || strncmp(buf, "ERROR", 5) == 0) {
              done = 1;
            }
            // e.g. +WIFI: wifi=up ip=192.168.1.10 port=80
            if (strncmp(buf, "+WIFI:", 6) == 0) {
              char *ip_ptr = strstr(buf, "ip=");
              char *port_ptr = strstr(buf, "port=");
              if (ip_ptr && port_ptr) {
                ip_ptr += 3;
                port_ptr += 5;
                char ip_str[32] = {0};
                char port_str[16] = {0};
                int j = 0;
                while (ip_ptr[j] && ip_ptr[j] != ' ' && j < 31) { ip_str[j] = ip_ptr[j]; j++; }
                j = 0;
                while (port_ptr[j] && port_ptr[j] != ' ' && j < 15) { port_str[j] = port_ptr[j]; j++; }
                
                strcpy(wifi_url, "http://");
                strcat(wifi_url, ip_str);
                strcat(wifi_url, ":");
                strcat(wifi_url, port_str);
              }
            }
            buf_idx = 0;
          }
        } else {
          if (buf_idx < 127) buf[buf_idx++] = c;
        }
      }
    }
  }
}

#define MAX_CORES 50
char cores[MAX_CORES][80];
int core_count = 0;

void read_cores() {
  print_text80(2, 4, 0x07, "Querying AT+CORELIST...              ");
  send_cmd("AT+CORELIST");
  
  char buf[128];
  int buf_idx = 0;

  uint32_t frames_passed = 0;
  uint8_t last_raster = PEEK(0xD012);
  
  // Wait up to 5 seconds of no data for the list to finish sending
  while(frames_passed < 250) {
    uint8_t current_raster = PEEK(0xD012);
    if (current_raster < last_raster) frames_passed++;
    last_raster = current_raster;

    uint8_t rx_buf[64];
    uint16_t count = modem_uart_read(rx_buf, sizeof(rx_buf));
    if (count > 0) {
      frames_passed = 0;
      for (uint16_t i=0; i<count; i++) {
        uint8_t c = rx_buf[i];
        debug_char(c);
        if (c == '\r' || c == '\n') {
          if (buf_idx > 0) {
            buf[buf_idx] = 0;
            // Ignore the command echo and the "OK L /" header
            if (strncmp(buf, "AT+CORELIST", 11) != 0 && strncmp(buf, "OK L /", 6) != 0) {
               // Only add it if it's not exactly "OK" which might come at the very end
               if (strcmp(buf, "OK") != 0 && core_count < MAX_CORES) {
                 strncpy(cores[core_count++], buf, 79);
               }
            }
            buf_idx = 0;
          }
        } else {
          if (buf_idx < 127) buf[buf_idx++] = c;
        }
      }
    }
  }
}

int parse_core_num(const char* str) {
  int num = 0;
  while(*str == ' ' || *str == '\t') str++;
  while(*str >= '0' && *str <= '9') {
    num = num * 10 + (*str - '0');
    str++;
  }
  return num;
}

int main(void)
{
  mega65_io_enable();
  POKE(0xd020,0);
  POKE(0xd021,0);  
  
  // Install NMI and BRK catchers from megacom.c
  POKE(0x0316,(uint8_t)(((uint16_t)&brk_catcher)>>0));
  POKE(0x0317,(uint8_t)(((uint16_t)&brk_catcher)>>8));
  POKE(0x0318,(uint8_t)(((uint16_t)&nmi_catcher)>>0));
  POKE(0x0319,(uint8_t)(((uint16_t)&nmi_catcher)>>8));
  
  h640_text_mode();

  print_box(0,0,79,49,0x01);
  print_text80(2,1,0x0e,"MEGA65 JTAG Core Loader");
  print_text80(40,1,0x0e,"UART Traffic:");

  // UART 0 at 2mbps
  modem_setup_serial(0, (40500000 / 2000000) - 1);

  wait_for_modem();
  query_info();
  read_cores();

  lfill(0xf000+80*4, 0x20, 80); // Clear querying text
  lfill(0xff80000L+80*4, 0x0e, 80); 

  // Print ATI info
  print_text80(2, 4, 0x06, ati_version);
  print_text80(2, 5, 0x0a, ati_build);
  print_text80(2, 6, 0x0e, ati_identity);
  print_text80(2, 7, 0x0e, ati_sdcard);
  print_text80(2, 8, 0x0e, ati_wifi_hw);
  
  if (wifi_url[0]) {
    char wbuf[80];
    strcpy(wbuf, "Web UI: ");
    strcat(wbuf, wifi_url);
    print_text80(2, 10, 0x03, wbuf);
  }

  if (core_count == 0) {
    print_text80(2, 12, 0x02, "No cores found or failed to read.");
  } else {
    print_text80(2, 12, 0x0a, "Select a core and press RETURN or FIRE");
  }

  int selected = 0;
  int top_idx = 0;
  int page_size = 30;
  
  uint8_t last_joy = 0xff;

  while(1) {
    for (int i=0; i<page_size; i++) {
      int c_idx = top_idx + i;
      char disp[40];
      if (c_idx < core_count) {
        if (c_idx == selected) {
          char tmp[80];
          strcpy(tmp, " > ");
          strncat(tmp, cores[c_idx], 35);
          print_text80(2, 14+i, 0x01, tmp); // highlight
        } else {
          char tmp[80];
          strcpy(tmp, "   ");
          strncat(tmp, cores[c_idx], 35);
          print_text80(2, 14+i, 0x0e, tmp);
        }
      } else {
        lfill(0xf000+80*(14+i)+2, 0x20, 36);
        lfill(0xff80000L+80*(14+i)+2, 0x0e, 36);
      }
    }

    uint8_t joy = PEEK(0xDC00);
    uint8_t joy_changed = (joy != last_joy);
    last_joy = joy;
    
    int move_up = 0;
    int move_down = 0;
    int do_launch = 0;
    
    if (joy_changed) {
      if ((joy & 0x01) == 0) move_up = 1;
      if ((joy & 0x02) == 0) move_down = 1;
      if ((joy & 0x10) == 0) do_launch = 1;
    }

    if (PEEK(0xD610)) {
      uint8_t key = PEEK(0xD610);
      POKE(0xD610, 0);
      
      if (key == 0x11) move_down = 1; // cursor down
      if (key == 0x91) move_up = 1; // cursor up
      if (key == 0x0d) do_launch = 1; // return
    }
    
    if (move_down) {
      if (selected < core_count - 1) {
        selected++;
        if (selected >= top_idx + page_size) top_idx++;
      }
    } else if (move_up) {
      if (selected > 0) {
        selected--;
        if (selected < top_idx) top_idx--;
      }
    } else if (do_launch) {
      if (core_count > 0) {
        char cmd[80];
        int core_num = parse_core_num(cores[selected]);
        char nbuf[10];
        sprintf(nbuf, "%d", core_num);
        strcpy(cmd, "AT+JTAGLOAD=");
        strcat(cmd, nbuf);
        send_cmd(cmd);
        print_text80(2, 47, 0x0a, "Command sent:                ");
        print_text80(16, 47, 0x0a, cmd);
      }
    }
  }

  return 0;
}
