#include "includes.h"

#include "screen.h"
#include "status.h"

uint8_t registered_with_network = 1;
uint8_t signal_level = 0xb0;
uint8_t battery_percent = 100;
uint8_t is_charging = 0;

unsigned char signal_none[]="📵";
// The following two strings must use unicode symbols that encode to the same number of bytes as each other.
// The filler chars after must be exactly 7px wide to keep alignment
unsigned char signal_strength[]="📱cccc";
unsigned char signal_strength_forbidden[]="🚫cccc";

unsigned char battery_charging[]="⚡ccccc";
unsigned char battery_fullish[]=" 🔋ccccc";
unsigned char battery_discharging[]=" 🪫ccccc";
unsigned char battery_flat[]=" ⚠🪫";
unsigned char battery_flat_charging[]=" ⚡🪫";

unsigned char status_time[32];

void statusbar_draw_battery(void)
{
  // Battery status as percentage
  // Signal strength
  uint8_t *battery_string = NULL;
  uint8_t bars = 0;
  if (battery_percent <20) {
    if (is_charging) battery_string = battery_flat_charging;
    else battery_string = battery_flat;
  }
  else {
    if (is_charging) battery_string = battery_charging;
    else battery_string = battery_discharging;
    bars = battery_percent/19;
    if (bars>5) bars=5;
    if (bars>=4) battery_string = battery_fullish;
  }
  draw_string_nowrap(ST_GL_BATTERY_START,1,
		     FONT_UI,
		     0x81, // reverse white
		     battery_string,
		     ST_PX_BATTERY_START,
		     ST_PX_BATTERY,
		     ST_GL_BATTERY_START+ST_GL_BATTERY,
		     NULL,
		     VIEWPORT_PADDED,
		     NULL,NULL);
  if (battery_percent >= 20) {
    // Then we munge the c characters to instead show our battery charge level bars
    for(uint8_t bar = 0; bar < 5; bar++) {
      // Draw bar or lack of bar
      lpoke(screen_ram + 1 * (2 * 0x100) + (ST_GL_BATTERY_START+3 + bar)*2 + 0, 0x6f);
      // Choose non-FCM glyph and trim to 7px wide
      lpoke(screen_ram + 1 * (2 * 0x100) + (ST_GL_BATTERY_START+3 + bar)*2 + 1, 0x20);
    }
  }
  
}

void statusbar_draw_signal(void)
{
  // Signal strength
  uint8_t *signal_string = NULL;
  uint8_t bars = 0;
  if (signal_level == 0) signal_string = signal_none;
  else {
    signal_string = signal_strength;
    if (!registered_with_network) signal_string = signal_strength_forbidden;
    bars = signal_level/50;
    if (bars>4) bars=4;
  }
  draw_string_nowrap(ST_GL_SIGNAL_START,1,
		     FONT_UI,
		     0x81, // reverse white
		     signal_string,
		     ST_PX_SIGNAL_START,
		     ST_PX_SIGNAL,
		     ST_GL_SIGNAL_START+ST_GL_SIGNAL,
		     NULL,
		     VIEWPORT_PADDED,
		     NULL,NULL);
  // Then we munge the _ characters to instead show our signal strength indicators
  for(uint8_t bar = 0; bar < 4; bar++) {
    // Draw bar or lack of bar
    lpoke(screen_ram + 1 * (2 * 0x100) + (ST_GL_SIGNAL_START+2 + bar)*2 + 0, (bar < bars) ? 0x71 + bar*2 : 0x79 + bar * 2);
    // Choose non-FCM glyph and trim to 7px wide
    lpoke(screen_ram + 1 * (2 * 0x100) + (ST_GL_SIGNAL_START+2 + bar)*2 + 1, 0x20);
  }
}

void statusbar_draw_indicators(void)
{
  // Reserved for status indicators
  draw_string_nowrap(ST_GL_INDICATORS_START,1,
		     FONT_UI,
		     0x81, // reverse white
		     (unsigned char *)"",
		     ST_PX_INDICATORS_START,
		     ST_PX_INDICATORS,
		     ST_GL_INDICATORS_START+ST_GL_INDICATORS,
		     NULL,
		     VIEWPORT_PADDED,
		     NULL,NULL);
}


void statusbar_draw_time(void)
{
  // Hour
  bcd_to_str(lpeek(0xffd7112L)&0x7f,&status_time[0]);
  status_time[2]=':';
  // minute
  bcd_to_str(lpeek(0xffd7111L),&status_time[3]);

#ifdef SHOW_SECONDS
  // seconds
  status_time[5]=':';
  bcd_to_str(lpeek(0xffd7110L),&status_time[6]);
  status_time[8]=0;
#else
  status_time[5]=0;
#endif
  
  draw_string_nowrap(0,1,
		     FONT_UI,
		     0x81, // reverse white
		     status_time,
		     0,ST_PX_TIME,ST_GL_TIME,
		     NULL,
		     VIEWPORT_PADDED,
		     NULL,NULL);

  return;
}

void statusbar_draw_netname(void)
{
  // Cellular Network name
  draw_string_nowrap(ST_GL_TIME,1,
		     FONT_UI,
		     0x81, // reverse white
		     (unsigned char *)"MEGAtel",
		     ST_PX_TIME,ST_PX_NETNAME,
		     ST_GL_TIME+ST_GL_NETNAME,
		     NULL,
		     VIEWPORT_PADDED,
		     NULL,NULL);
}

void statusbar_draw_reserved(void)
{
  // While fill for space in between
  draw_string_nowrap(ST_GL_RESERVED_START,1,
		     FONT_UI,
		     0x81, // reverse white
		     (unsigned char *)"",
		     ST_PX_RESERVED_START,
		     ST_PX_RESERVED,
		     ST_GL_RESERVED_START+ST_GL_RESERVED,
		     NULL,
		     VIEWPORT_PADDED,
		     NULL,NULL);
}

void statusbar_setup(void)
{
  // Prevent display glitching from RRB wrap etc in status bar
  for(uint8_t x=1;x<RENDER_COLUMNS-2;x++) draw_goto(x,1,704);

  // Setup signal strength increment mono glyphs
  uint8_t toggle = 0x00;
  for(uint8_t level=0;level<8;level++) {
    for(uint8_t y=0;y<8;y++) {
      // Bar
      lpoke(0xff7e000L + (0x70 + level)*8 + 7 - y, (y==0||y>level||y==7) ? 0x00 : 0xff);
      lpoke(0xff7e800L + (0x70 + level)*8 + 7 - y, (y==0||y>level) ? 0x00 : 0xff);
      // Empty bar
      lpoke(0xff7e000L + (0x78 + level)*8 + 7 - y, (y==0||y>level||y==7) ? 0x00 : 0xaa ^ toggle );
      lpoke(0xff7e800L + (0x78 + level)*8 + 7 - y, (y==0||y>level) ? 0x00 : 0x55 ^ toggle);
      // Battery level bars
      lpoke(0xff7e000L + (0x6f)*8 + 7 - y, (y==0||y==7) ? 0x00 : 0xfc );
      lpoke(0xff7e800L + (0x6f)*8 + 7 - y, (y==0||y==7) ? 0x00 : 0xfc );
    }

    // Toggle prevents the hashed empty bars from having matching edges
    if (level&1) toggle = toggle ^ 0xff;
  }
}  

void statusbar_draw()
{
  // Time on the left
  statusbar_draw_time();

  statusbar_draw_netname();
  statusbar_draw_reserved();
  statusbar_draw_indicators();
  statusbar_draw_signal();
  statusbar_draw_battery();

  
}

		    
