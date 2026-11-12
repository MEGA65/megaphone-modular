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
  while (*msg) {
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

// Like print_text40, but blanks the rest of the row so no stale text remains
void print_line40(unsigned char y, unsigned char colour, const char *msg)
{
  char line[41];
  uint8_t i = 0;
  while (i < 40 && msg[i]) { line[i] = msg[i]; i++; }
  while (i < 40) line[i++] = ' ';
  line[40] = 0;
  print_text40(0, y, colour, 0, line);
}

void c64_40col_mode(void)
{
  // Switch to standard lowercase/uppercase C64 mode
  POKE(0xD031, 0x40);

  POKE(0xD018, 0x16); // Screen RAM $0400, Char ROM $1800
  POKE(0xD058, 40);   // Logical row width
  POKE(0xD05E, 40);
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

// ---------------------------------------------------------------------------
// Robust AT command handling
// ---------------------------------------------------------------------------

static uint8_t at_timed_out = 0;

// Discard anything already waiting in the UART. Normally waits for 2 frames
// of silence; after a timeout waits for 1 s, since a late reply is likely
// still on its way.
static void uart_drain(void) {
  uint8_t need = at_timed_out ? 50 : 2;
  at_timed_out = 0;
  uint8_t tmp[64];
  uint8_t last = PEEK(0xD012), quiet = 0;
  while (quiet < need) {
    if (modem_uart_read(tmp, sizeof tmp)) quiet = 0;
    uint8_t r = PEEK(0xD012);
    if (r < last) quiet++;
    last = r;
  }
}

typedef void (*line_handler)(const char *line);

// Sends cmd and passes each response line (except its own echo) to on_line.
// Returns 1 = OK, 0 = ERROR/ERR (line kept in at_error_line), -1 = timeout
// (250 frames of silence)
static char at_error_line[41];

static int at_command(const char *cmd, line_handler on_line) {
  static char buf[256];
  int idx = 0;
  at_error_line[0] = 0;
  uart_drain();
  send_cmd((char *)cmd);

  uint16_t frames = 0;
  uint8_t last = PEEK(0xD012);
  while (frames < 250) {
    uint8_t r = PEEK(0xD012);
    if (r < last) frames++;
    last = r;

    uint8_t rx[64];
    uint16_t n = modem_uart_read(rx, sizeof rx);
    if (n) frames = 0;
    for (uint16_t i = 0; i < n; i++) {
      uint8_t c = rx[i];
      if (c == '\r' || c == '\n') {
        if (!idx) continue;
        buf[idx] = 0; idx = 0;
        if (strncmp(buf, "OK", 2) == 0)    return 1;
        if (strncmp(buf, "ERR", 3) == 0) {     // "ERROR" or "ERR P ..."
          strncpy(at_error_line, buf, 40);
          at_error_line[40] = 0;
          return 0;
        }
        if (strcmp(buf, cmd) == 0) continue;   // our own echo only
        if (on_line) on_line(buf);
      } else if (idx < 255) {
        buf[idx++] = c;
      }
    }
  }
  at_timed_out = 1;
  return -1;
}

void wait_for_modem() {
  char buf[80];
  int buf_idx = 0;

  while (1) {
    print_text40(0, 2, 0x0a, 0, "Waiting for JTAG modem to respond...    ");
    send_cmd("ATZ");

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
              if (strncmp(buf, "OK READY", 8) == 0) {
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

// ---------------------------------------------------------------------------
// Modem info (ATI / AT+WIFI?)
// ---------------------------------------------------------------------------

char ati_version[80] = "";
char ati_build[80] = "";
char ati_identity[80] = "";
char ati_sdcard[80] = "";
char ati_wifi_hw[80] = "";
char wifi_url[80] = "";

// Temporaries: filled while parsing, committed only from a genuine response
static char t_version[40], t_build[40], t_identity[40], t_sdcard[40], t_wifi_hw[40];
static char t_url[80];
static int  t_line;
static uint8_t t_wifi_seen;

static void ati_line(const char *l) {
  if (t_line++ == 0)                        strncpy(t_version,  l, 39);
  else if (strncmp(l, "BUILD:", 6) == 0)    strncpy(t_build,    l, 39);
  else if (strncmp(l, "IDENTITY:", 9) == 0) strncpy(t_identity, l, 39);
  else if (strncmp(l, "SDCARD:", 7) == 0)   strncpy(t_sdcard,   l, 39);
  else if (strncmp(l, "WIFI:", 5) == 0)     strncpy(t_wifi_hw,  l, 39);
}

// e.g. +WIFI: wifi=up ip=192.168.1.10 port=80
static void wifi_line(const char *l) {
  if (strncmp(l, "+WIFI:", 6) != 0) return;
  t_wifi_seen = 1;
  const char *ip = strstr(l, "ip="), *port = strstr(l, "port=");
  if (!ip || !port) return;
  ip += 3; port += 5;
  char ip_str[32] = {0}, port_str[16] = {0};
  for (int j = 0; ip[j]   && ip[j]   != ' ' && j < 31; j++) ip_str[j]   = ip[j];
  for (int j = 0; port[j] && port[j] != ' ' && j < 15; j++) port_str[j] = port[j];
  strcpy(t_url, "http://");
  strcat(t_url, ip_str);
  strcat(t_url, ":");
  strcat(t_url, port_str);
}

// SD card generation: the firmware bumps it every time the card is mounted or
// unmounted, so a remove/reinsert between our polls still changes it.
uint32_t sd_gen = 0;
uint8_t  sd_gen_known = 0;     // 0 on firmware that doesn't report gen=
static uint32_t t_gen;
static uint8_t  t_gen_seen;

// e.g. +SDCARD: state=active mounted=1 config_loaded=1 transport=hw gen=3
static void sdcard_line(const char *l) {
  if (strncmp(l, "+SDCARD:", 8) != 0) return;
  const char *g = strstr(l, "gen=");
  if (!g || g[4] < '0' || g[4] > '9') return;
  t_gen = 0;
  for (g += 4; *g >= '0' && *g <= '9'; g++) t_gen = t_gen * 10 + (*g - '0');
  t_gen_seen = 1;
}

void query_info(uint8_t quiet) {
  if (!quiet) print_text40(0, 3, 0x07, 0, "Querying ATI...                         ");
  memset(t_version, 0, sizeof t_version);
  memset(t_build, 0, sizeof t_build);
  memset(t_identity, 0, sizeof t_identity);
  memset(t_sdcard, 0, sizeof t_sdcard);
  memset(t_wifi_hw, 0, sizeof t_wifi_hw);
  t_line = 0;

  if (at_command("ATI", ati_line) == 1 && t_build[0]) {
    // Only a genuine ATI response contains BUILD:, so stale "OK"s are rejected.
    // Static fields: refresh, but never clear
    if (t_version[0])  strcpy(ati_version,  t_version);
    strcpy(ati_build, t_build);
    if (t_identity[0]) strcpy(ati_identity, t_identity);
    // Dynamic fields: update only when the modem actually reported them
    if (t_sdcard[0])   strcpy(ati_sdcard,   t_sdcard);
    if (t_wifi_hw[0])  strcpy(ati_wifi_hw,  t_wifi_hw);
  }

  t_gen_seen = 0;
  if (at_command("AT+SDCARD?", sdcard_line) == 1 && t_gen_seen) {
    sd_gen = t_gen;
    sd_gen_known = 1;
  }

  if (!quiet) print_text40(0, 3, 0x07, 0, "Querying AT+WIFI?...                    ");
  t_url[0] = 0;
  t_wifi_seen = 0;
  if (at_command("AT+WIFI?", wifi_line) == 1 && t_wifi_seen)
    strcpy(wifi_url, t_url);   // empty if the line had no ip=, i.e. WiFi down
  // anything else (ERROR, timeout, stale OK): keep the previous value
}

// ---------------------------------------------------------------------------
// Core list
// ---------------------------------------------------------------------------

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

void set_max_send(uint16_t max_bytes) {
  char max_send_command[20];

  print_text40(0, 3, 0x07, 0, "Setting Max Send...                     ");
  strcpy(max_send_command, "AT+COREMAX=");
  if (max_bytes < 10) {
    max_send_command[11] = '0' + max_bytes; max_send_command[12] = 0;
  } else if (max_bytes < 100) {
    max_send_command[11] = '0' + (max_bytes/10); max_send_command[12] = '0' + (max_bytes%10); max_send_command[13] = 0;
  } else {
    max_send_command[11] = '0' + (max_bytes/100); max_send_command[12] = '0' + ((max_bytes/10)%10); max_send_command[13] = '0' + (max_bytes%10); max_send_command[14] = 0;
  }
  // send_cmd() adds the terminator, so no "\n" here
  at_command(max_send_command, 0);
}

uint8_t any_key_pressed(void) {
  for (uint8_t i = 0; i < 9; i++) {
    POKE(0xD614, i);
    uint8_t keys = PEEK(0xD613);
    if (keys != 0xff) {
      return 1;
    }
  }
  return 0;
}

void parse_core(const char* core_detail) {
  char *p = strstr(core_detail, "index=");
  if (p) {
    int num = parse_core_num(p + 6);
    if (num > 0 && core_count < MAX_CORES) {
      char *kind = strstr(core_detail, "kind=");
      char *path_ptr = strstr(core_detail, "path=\"");
      char *title = strstr(core_detail, "title=\"");

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

// Returns 0 if the firmware could not list the folder (e.g. it was deleted)
int read_cores(const char* path) {
  core_count = 0;
  int failed = 0;

  set_max_send(150);

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
  uart_drain();
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
            } else if (strncmp(buf, "ERR", 3) == 0) {
               // "ERR L ..." has no END after it: stop now, not after a timeout
               failed = 1;
               done = 1;
            } else if (strncmp(buf, "CONT", 4) == 0) {
              send_cmd("AT+COREMORE");
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

  if (failed) {
    set_max_send(0);
    return 0;
  }

  // PASS 2: AT+COREDETAIL to get COREs
  strcpy(cmd, "AT+COREDETAIL");
  if (strcmp(path, "/") != 0) {
    strcat(cmd, "=");
    strcat(cmd, path);
  }
  print_text40(0, 3, 0x07, 0, "Querying cores...                       ");
  uart_drain();
  send_cmd(cmd);

  buf_idx = 0;
  frames_passed = 0;
  last_raster = PEEK(0xD012);
  done = 0;

  static char detail_buf[512];
  memset(detail_buf, 0, sizeof(detail_buf));
  uint8_t appending = 0;
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
            } else if (strncmp(buf, "ERR", 3) == 0) {
              done = 1;   // as above: no END follows an error
            } else if (strncmp(buf, "CONT", 4) == 0) {
              send_cmd("AT+COREMORE");
            } else if (strncmp(buf, "+COREDETAIL:", 12) == 0) {
              strncpy(detail_buf, buf, sizeof(detail_buf) - 1);
              detail_buf[sizeof(detail_buf) - 1] = 0;
              if (detail_buf[strlen(detail_buf) - 1] == '\\') {
                appending = 1;
                detail_buf[strlen(detail_buf) - 1] = 0;
              } else {
                parse_core(detail_buf);
              }
            } else if ((strncmp(buf, "AT+COREMORE", 11) != 0) && (strncmp(buf, "OK", 2) != 0) && appending) {
              strncat(detail_buf, buf, sizeof(detail_buf) - 1);
              detail_buf[sizeof(detail_buf) - 1] = 0;
              if (detail_buf[strlen(detail_buf) - 1] == '\\') {
                detail_buf[strlen(detail_buf) - 1] = 0;
              } else {
                appending = 0;
                parse_core(detail_buf);
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

  set_max_send(0);
  return 1;
}

// "ACTIVE" = mounted, "PRESENT" = inserted but not mounted yet (e.g. right
// after power-up). No SDCARD: line at all (older firmware) counts as present.
static uint8_t sd_present(void) {
  return !ati_sdcard[0] || strncmp(ati_sdcard, "SDCARD: NOT PRESENT", 19) != 0;
}

static uint8_t sd_mounted(void) {
  return !ati_sdcard[0] || strncmp(ati_sdcard, "SDCARD: ACTIVE", 14) == 0;
}

void print_ati_info() {
  // Clear querying text
  print_text40(0, 3, 0x01, 0, "                                        ");

  int info_row = 2;
  print_line40(info_row++, 0x05, ati_version);
  if (ati_build[0]) print_line40(info_row++, 0x0a, ati_build);
  if (ati_identity[0]) print_line40(info_row++, 0x0e, ati_identity);

  if (!sd_mounted()) {
    print_line40(info_row++, 0x02, ati_sdcard);
  }
  if (ati_wifi_hw[0] && strncmp(ati_wifi_hw, "WIFI: BUILT-IN", 14) != 0) {
    print_line40(info_row++, 0x02, ati_wifi_hw);
  }

  if (wifi_url[0]) {
    char wbuf[40];
    strcpy(wbuf, "Web: ");
    strncat(wbuf, wifi_url, 34);
    print_line40(info_row++, 0x03, wbuf);
  }

  // Clear any remaining rows before the core list starts at row 9
  while (info_row < 9) {
    print_line40(info_row++, 0x01, "");
  }

  if (!sd_present()) {
    print_text40(0, 9, 0x02, 0, "Insert an SD card to list cores.        ");
  } else if (core_count == 0) {
    print_text40(0, 9, 0x02, 0, "No cores found or failed to read.       ");
  } else {
    print_text40(0, 9, 0x0a, 0, "Select a core and press RETURN or FIRE: ");
  }
}

// ---------------------------------------------------------------------------
// Periodic status re-probe
// ---------------------------------------------------------------------------

// The SD card can be pulled at any moment, so poll often
#define PROBE_FRAMES  (50 * 2)   // every 2 s

static uint16_t probe_frames = 0;
static uint8_t  probe_last_raster = 0;
static uint8_t  jtag_sent = 0;         // no probing once a core load is sent

static uint32_t list_gen = 0;          // sd_gen when the core list was read
static char load_error[41] = "";       // shown on row 9 after the next redraw
static char keep_sel[80] = "";         // entry to reselect after a reload

// Returns 1 if the SD card was inserted, removed or swapped since the core
// list was read
static uint8_t refresh_status(void) {
  char old_wifi[80], old_url[80], old_sd[80];
  uint8_t was_present = sd_present();
  strcpy(old_wifi, ati_wifi_hw);
  strcpy(old_url, wifi_url);
  strcpy(old_sd, ati_sdcard);
  query_info(1);   // quiet: no "Querying..." flicker
  if (sd_gen_known && sd_gen != list_gen) return 1;  // caller redraws everything
  if (sd_present() != was_present) return 1;   // older firmware: no gen=
  if (strcmp(old_wifi, ati_wifi_hw) || strcmp(old_url, wifi_url) || strcmp(old_sd, ati_sdcard))
    print_ati_info();
  return 0;
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

  // UART 0 at 115.2k
  modem_setup_serial(0, (40500000 / 115200 + 1) - 1);

  wait_for_modem();
  query_info(0);

  char current_path[128] = "/";

  while(1) {
    // Inserted but not mounted: mount it now, so the gen sampled below is the
    // one the listing runs under (otherwise listing mounts it, bumps gen, and
    // the next poll pointlessly reloads)
    if (sd_present() && !sd_mounted()) {
      at_command("AT+MOUNT", 0);
      query_info(1);
    }

    // Sample the generation before reading, as the web page does: if the card
    // changes mid-read, the next poll sees a newer gen and reloads again
    list_gen = sd_gen;

    if (sd_present()) {
      // Stay in the current folder across changes (an upload, a delete, even
      // a card swap); fall back to the root only if the folder is now gone
      if (!read_cores(current_path) && strcmp(current_path, "/") != 0) {
        strcpy(current_path, "/");
        keep_sel[0] = 0;
        read_cores(current_path);
      }
    } else {
      core_count = 0;   // card gone: the old list is invalid
      strcpy(current_path, "/");
      keep_sel[0] = 0;
    }
    print_ati_info();
    if (load_error[0]) {
      print_line40(9, 0x02, load_error);
      load_error[0] = 0;
    }

    // A directory reload just talked to the modem; restart the probe timer
    probe_frames = 0;
    probe_last_raster = PEEK(0xD012);

    int selected = 0;
    int top_idx = 0;
    int page_size = 14;

    // After a reload caused by a change on the card, stay on the same entry.
    // Numbers can shift, so match on the text after the number.
    if (keep_sel[0]) {
      for (int i = 0; i < core_count; i++) {
        if (strcmp(cores[i] + 4, keep_sel) == 0) { selected = i; break; }
      }
      keep_sel[0] = 0;
      if (selected >= page_size) top_idx = selected - page_size + 1;
    }
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

      // Periodic status probe
      uint8_t r = PEEK(0xD012);
      if (r < probe_last_raster) probe_frames++;
      probe_last_raster = r;
      if (!jtag_sent && probe_frames >= PROBE_FRAMES) {
        if (refresh_status()) {
          // Card swapped/removed or its files changed: reload the core list
          if (selected < core_count) strcpy(keep_sel, cores[selected] + 4);
          break_to_refresh = 1;
          continue;
        }
        probe_frames = 0;
        probe_last_raster = PEEK(0xD012);
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
            // The firmware refuses a numbered load if the card changed since
            // our AT+CORELIST, so a pull inside the poll window can't load
            // the wrong core. Returns on "OK P OPEN" or "ERR P ...".
            if (at_command(cmd, 0) == 0) {
              if (strstr(at_error_line, "sd card changed"))
                strcpy(load_error, "Files changed; list reloaded.");
              else
                strcpy(load_error, at_error_line);
              query_info(1);
              if (selected < core_count) strcpy(keep_sel, cores[selected] + 4);
              break_to_refresh = 1;
              continue;
            }
            // OK, or no reply (the load may have reset us): stop talking
            jtag_sent = 1;
            print_text40(0, 24, 0x0a, 0, "Command sent:");
            print_text40(14, 24, 0x0e, 0, cmd);
          }
        }
      }
    }
  }

  return 0;
}
