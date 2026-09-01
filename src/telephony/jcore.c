#include "includes.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "uart.h"

uint8_t ascii_to_screencode(uint8_t c) {
  if (c >= 64 && c <= 95) {
     if (c == 64) return 0;
     if (c >= 65 && c <= 90) return c; // 'A'-'Z' -> 65-90
     if (c >= 91 && c <= 95) return c - 64;
  }
  if (c >= 96 && c <= 127) {
     if (c >= 97 && c <= 122) return c - 96; // 'a'-'z' -> 1-26
     return c - 96; 
  }
  return c;
}

void print_text40(unsigned char x, unsigned char y, unsigned char colour, unsigned char reverse, const char *msg)
{
  uint16_t addr = 0x0400 + x + y * 40;
  uint16_t caddr = 0xD800 + x + y * 40;
  while (*msg && x < 40) {
    uint8_t sc = ascii_to_screencode(*msg);
    if (reverse) sc |= 0x80;
    POKE(addr, sc);
    POKE(caddr, colour);
    msg++;
    addr++;
    caddr++;
    x++;
  }
}

void c64_40col_mode(void)
{
  // Switch to standard lowercase/uppercase C64 mode
  POKE(0xD018, 0x16); // Screen RAM $0400, Char ROM $1800
  POKE(0xD058, 40);   // Logical row width
  POKE(0xD011, PEEK(0xD011) & ~0x80); // clear high bit of raster
  POKE(0xD016, 0xC8); // 40 cols
  
  // Clear screen
  for(int i=0; i<1000; i++) {
    POKE(0x0400 + i, 0x20); // space
    POKE(0xD800 + i, 0x01); // white
  }
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

void wait_for_modem() {
  char buf[80];
  int buf_idx = 0;
  
  while (1) {
    print_text40(0, 2, 0x0a, 0, "Waiting for JTAG modem to respond...    ");
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
      print_text40(0, 2, 0x05, 0, "Modem OK!                               ");
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
  print_text40(0, 3, 0x07, 0, "Querying ATI...                         ");
  send_cmd("ATI");
  
  char buf[128];
  int buf_idx = 0;
  int line_count = 0;
  
  uint32_t frames_passed = 0;
  uint8_t last_raster = PEEK(0xD012);
  int done = 0;
  
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
        if (c == '\r' || c == '\n') {
          if (buf_idx > 0) {
            buf[buf_idx] = 0;
            if (strncmp(buf, "OK", 2) == 0 || strncmp(buf, "ERROR", 5) == 0) {
              done = 1;
            }
            if (strncmp(buf, "ATI", 3) != 0 && !done) {
              if (line_count == 0) {
                strncpy(ati_version, buf, 39);
              } else if (strncmp(buf, "BUILD:", 6) == 0) {
                strncpy(ati_build, buf, 39);
              } else if (strncmp(buf, "IDENTITY:", 9) == 0) {
                strncpy(ati_identity, buf, 39);
              } else if (strncmp(buf, "SDCARD:", 7) == 0) {
                strncpy(ati_sdcard, buf, 39);
              } else if (strncmp(buf, "WIFI:", 5) == 0) {
                strncpy(ati_wifi_hw, buf, 39);
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

  print_text40(0, 3, 0x07, 0, "Querying AT+WIFI?...                    ");
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

int parse_core_num(const char* str) {
  int num = 0;
  while(*str == ' ' || *str == '\t') str++;
  while(*str >= '0' && *str <= '9') {
    num = num * 10 + (*str - '0');
    str++;
  }
  return num;
}

void read_cores(const char* path) {
  core_count = 0;
  
  if (strcmp(path, "/") != 0) {
    strcpy(cores[core_count++], "  0 DIR  /");
    strcpy(cores[core_count++], "  0 DIR  ..");
  }
  
  char cmd[128];
  strcpy(cmd, "AT+CORELIST");
  if (strcmp(path, "/") != 0) {
    strcat(cmd, "=");
    strcat(cmd, path);
  }
  
  print_text40(0, 3, 0x07, 0, "Querying directories...                 ");
  send_cmd(cmd);
  
  static char buf[512];
  int buf_idx = 0;
  uint32_t frames_passed = 0;
  uint8_t last_raster = PEEK(0xD012);
  int done = 0;

  // PASS 1: AT+CORELIST to get DIRs
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
        if (c == '\r' || c == '\n') {
          if (buf_idx > 0) {
            buf[buf_idx] = 0;
            if (strncmp(buf, "END", 3) == 0) {
               done = 1;
            } else if (strncmp(buf, "AT+CORELIST", 11) != 0 && strncmp(buf, "OK", 2) != 0) {
               int num = parse_core_num(buf);
               if (num > 0 && core_count < MAX_CORES) {
                 char *dir_ptr = strstr(buf, "DIR ");
                 if (dir_ptr) {
                   char *p = dir_ptr + 4;
                   while(*p == ' ') p++;
                   while(*p >= '0' && *p <= '9') p++; // skip size
                   while(*p == ' ') p++;
                   if (*p == '-') {
                     p++;
                     while(*p == ' ') p++;
                   } else if (*p >= '0' && *p <= '9') {
                     while(*p && *p != ' ') p++; // skip date
                     while(*p == ' ') p++;
                     while(*p && *p != ' ') p++; // skip time
                     while(*p == ' ') p++;
                   }
                   if (*p) {
                     char nice[80] = {0};
                     char num_str[10];
                     if (num < 10) {
                       num_str[0] = ' '; num_str[1] = ' '; num_str[2] = '0' + num; num_str[3] = 0;
                     } else if (num < 100) {
                       num_str[0] = ' '; num_str[1] = '0' + (num/10); num_str[2] = '0' + (num%10); num_str[3] = 0;
                     } else {
                       num_str[0] = '0' + (num/100); num_str[1] = '0' + ((num/10)%10); num_str[2] = '0' + (num%10); num_str[3] = 0;
                     }
                     strcpy(nice, num_str);
                     strcat(nice, " DIR  ");
                     strncat(nice, p, 79 - strlen(nice));
                     strncpy(cores[core_count++], nice, 79);
                   }
                 }
               }
            }
            buf_idx = 0;
          }
        } else {
          if (buf_idx < 511) buf[buf_idx++] = c;
        }
      }
    }
  }

  // PASS 2: AT+COREDETAIL to get COREs
  strcpy(cmd, "AT+COREDETAIL");
  if (strcmp(path, "/") != 0) {
    strcat(cmd, "=");
    strcat(cmd, path);
  }
  print_text40(0, 3, 0x07, 0, "Querying cores...                       ");
  send_cmd(cmd);

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
        if (c == '\r' || c == '\n') {
          if (buf_idx > 0) {
            buf[buf_idx] = 0;
            if (strncmp(buf, "END", 3) == 0) {
               done = 1;
            } else if (strncmp(buf, "+COREDETAIL:", 12) == 0) {
               char *p = strstr(buf, "index=");
               if (p) {
                 int num = parse_core_num(p + 6);
                 if (num > 0 && core_count < MAX_CORES) {
                   char *kind = strstr(buf, "kind=");
                   char *path_ptr = strstr(buf, "path=\"");
                   char *title = strstr(buf, "title=\"");
                   
                   // Skip directories as we already got them!
                   int is_dir = 0;
                   if (kind && strncmp(kind + 5, "DIR", 3) == 0) is_dir = 1;
                   
                   if (!is_dir) {
                     char nice[80] = {0};
                     char name_buf[80] = {0};
                     
                     if (title) {
                       char *start = title + 7;
                       char *end = strchr(start, '"');
                       if (end && end > start) {
                         int len = end - start;
                         if (len > 29) len = 29;
                         strncpy(name_buf, start, len);
                       } else if (path_ptr) {
                         start = path_ptr + 6;
                         end = strchr(start, '"');
                         if (end) {
                           int len = end - start;
                           if (len > 29) len = 29;
                           strncpy(name_buf, start, len);
                         }
                       }
                     }
                     
                     char num_str[10];
                     if (num < 10) {
                       num_str[0] = ' '; num_str[1] = ' '; num_str[2] = '0' + num; num_str[3] = 0;
                     } else if (num < 100) {
                       num_str[0] = ' '; num_str[1] = '0' + (num/10); num_str[2] = '0' + (num%10); num_str[3] = 0;
                     } else {
                       num_str[0] = '0' + (num/100); num_str[1] = '0' + ((num/10)%10); num_str[2] = '0' + (num%10); num_str[3] = 0;
                     }
                     
                     strcpy(nice, num_str);
                     strcat(nice, " CORE ");
                     strncat(nice, name_buf, 79 - strlen(nice));
                     
                     strncpy(cores[core_count++], nice, 79);
                   }
                 }
               }
            }
            buf_idx = 0;
          }
        } else {
          if (buf_idx < 511) buf[buf_idx++] = c;
        }
      }
    }
  }
}

void print_ati_info() {
  // Clear querying text
  print_text40(0, 3, 0x01, 0, "                                        "); 

  int info_row = 2;
  print_text40(0, info_row++, 0x05, 0, ati_version);
  if (ati_build[0]) print_text40(0, info_row++, 0x0a, 0, ati_build);
  if (ati_identity[0]) print_text40(0, info_row++, 0x0e, 0, ati_identity);
  
  if (ati_sdcard[0] && strncmp(ati_sdcard, "SDCARD: ACTIVE", 14) != 0) {
    print_text40(0, info_row++, 0x02, 0, ati_sdcard);
  }
  if (ati_wifi_hw[0] && strncmp(ati_wifi_hw, "WIFI: BUILT-IN", 14) != 0) {
    print_text40(0, info_row++, 0x02, 0, ati_wifi_hw);
  }
  
  if (wifi_url[0]) {
    char wbuf[40];
    strcpy(wbuf, "Web: ");
    strncat(wbuf, wifi_url, 34);
    print_text40(0, info_row++, 0x03, 0, wbuf);
  }

  // Clear any remaining rows before the core list starts at row 9
  while (info_row < 9) {
    print_text40(0, info_row++, 0x01, 0, "                                        ");
  }

  if (core_count == 0) {
    print_text40(0, 9, 0x02, 0, "No cores found or failed to read.       ");
  } else {
    print_text40(0, 9, 0x0a, 0, "Select a core and press RETURN or FIRE: ");
  }
}

int main(void)
{
  mega65_io_enable();
  POKE(0xd020,0); // black border
  POKE(0xd021,0); // black bg
  
  // Install NMI and BRK catchers from megacom.c
  POKE(0x0316,(uint8_t)(((uint16_t)&brk_catcher)>>0));
  POKE(0x0317,(uint8_t)(((uint16_t)&brk_catcher)>>8));
  POKE(0x0318,(uint8_t)(((uint16_t)&nmi_catcher)>>0));
  POKE(0x0319,(uint8_t)(((uint16_t)&nmi_catcher)>>8));
  
  c64_40col_mode();

  print_text40(0, 0, 0x01, 1, " MEGA65 JTAG Core Loader                ");

  // UART 0 at 2mbps
  modem_setup_serial(0, (40500000 / 2000000) - 1);

  wait_for_modem();
  query_info();

  char current_path[128] = "/";

  while(1) {
    read_cores(current_path);
    print_ati_info();

    int selected = 0;
    int top_idx = 0;
    int page_size = 14;
    uint8_t last_joy = 0xff;
    int break_to_refresh = 0;

    while(!break_to_refresh) {
      for (int i=0; i<page_size; i++) {
        int c_idx = top_idx + i;
        char disp[41];
        if (c_idx < core_count) {
          strncpy(disp, cores[c_idx], 40);
          disp[40] = 0;
          for (int p=strlen(disp); p<40; p++) disp[p] = ' ';
          disp[40] = 0;
          
          if (c_idx == selected) {
            print_text40(0, 11+i, 0x01, 1, disp); // highlight reversed
          } else {
            print_text40(0, 11+i, 0x01, 0, disp); // normal
          }
        } else {
          print_text40(0, 11+i, 0x01, 0, "                                        ");
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
          if (selected >= top_idx + page_size) top_idx = selected - page_size + 1;
        }
      } else if (move_up) {
        if (selected > 0) {
          selected--;
          if (selected < top_idx) top_idx = selected;
        }
      } else if (do_launch) {
        if (core_count > 0) {
          char *dir_mark = strstr(cores[selected], " DIR  ");
          if (dir_mark) {
            char *dir_name = dir_mark + 6;
            if (strcmp(dir_name, "..") == 0) {
              char *last_slash = strrchr(current_path, '/');
              if (last_slash && last_slash != current_path) {
                *last_slash = 0;
              } else {
                strcpy(current_path, "/");
              }
            } else if (strcmp(dir_name, "/") == 0) {
              strcpy(current_path, "/");
            } else {
              if (strcmp(current_path, "/") != 0) strcat(current_path, "/");
              strcat(current_path, dir_name);
            }
            break_to_refresh = 1; // reload folder
          } else {
            // launch core
            char cmd[40];
            int core_num = parse_core_num(cores[selected]);
            char nbuf[10];
            
            if (core_num < 10) {
              nbuf[0] = '0' + core_num; nbuf[1] = 0;
            } else if (core_num < 100) {
              nbuf[0] = '0' + (core_num/10); nbuf[1] = '0' + (core_num%10); nbuf[2] = 0;
            } else {
              nbuf[0] = '0' + (core_num/100); nbuf[1] = '0' + ((core_num/10)%10); nbuf[2] = '0' + (core_num%10); nbuf[3] = 0;
            }
            
            strcpy(cmd, "AT+JTAGLOAD=");
            strcat(cmd, nbuf);
            send_cmd(cmd);
            print_text40(0, 24, 0x0a, 0, "Command sent:");
            print_text40(14, 24, 0x0e, 0, cmd);
          }
        }
      }
    }
  }

  return 0;
}
