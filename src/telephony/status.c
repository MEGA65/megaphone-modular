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

/*
  Status bar structure:

  0-63px (glyphs 0--15) = time
  64-191px (glyphs 16--39) = OS information
  
  64--703 (glyphs 16--156) = RESERVED

*/

unsigned char status_time[32];

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
		     0,64,16,
		     NULL,
		     VIEWPORT_PADDED,
		     NULL,NULL);

  return;
}

void statusbar_draw(void)
{
  // Prevent display glitching from RRB wrap etc
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
  

  // Time on the left
  statusbar_draw_time();

  // Cellular Network name
  draw_string_nowrap(16,1,
		     FONT_UI,
		     0x81, // reverse white
		     (unsigned char *)"MEGAtel",
		     64,128,
		     16+24,
		     NULL,
		     VIEWPORT_PADDED,
		     NULL,NULL);

  // While fill for space in between
  draw_string_nowrap(16+24,1,
		     FONT_UI,
		     0x81, // reverse white
		     (unsigned char *)"",
		     64+128,199,16+24+50,
		     NULL,
		     VIEWPORT_PADDED,
		     NULL,NULL);
  
  // Reserved for status indicators
  draw_string_nowrap(16+24+50,1,
		     FONT_UI,
		     0x81, // reverse white
		     (unsigned char *)"",
		     64+128+200,129,16+24+50+24,
		     NULL,
		     VIEWPORT_PADDED,
		     NULL,NULL);

  
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
  draw_string_nowrap(16+24+50+24,1,
		     FONT_UI,
		     0x81, // reverse white
		     signal_string,
		     64+128+200+129,48,16+24+50+24+8,
		     NULL,
		     VIEWPORT_PADDED,
		     NULL,NULL);
  // Then we munge the _ characters to instead show our signal strength indicators
  for(uint8_t bar = 0; bar < 4; bar++) {
    // Draw bar or lack of bar
    lpoke(screen_ram + 1 * (2 * 0x100) + (16+24+50+24+2 + bar)*2 + 0, (bar < bars) ? 0x71 + bar*2 : 0x79 + bar * 2);
    // Choose non-FCM glyph and trim to 7px wide
    lpoke(screen_ram + 1 * (2 * 0x100) + (16+24+50+24+2 + bar)*2 + 1, 0x20);
  }
  
  // Battery status as percentage
  // Signal strength
  uint8_t *battery_string = NULL;
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
  draw_string_nowrap(16+24+50+24+8,1,
		     FONT_UI,
		     0x81, // reverse white
		     battery_string,
		     64+128+200+129+48,64,16+24+50+24+8+16,
		     NULL,
		     VIEWPORT_PADDED,
		     NULL,NULL);
  if (battery_percent >= 20) {
    // Then we munge the c characters to instead show our battery charge level bars
    for(uint8_t bar = 0; bar < 5; bar++) {
      // Draw bar or lack of bar
      lpoke(screen_ram + 1 * (2 * 0x100) + (16+24+50+24+8+3 + bar)*2 + 0, 0x6f);
      // Choose non-FCM glyph and trim to 7px wide
      lpoke(screen_ram + 1 * (2 * 0x100) + (16+24+50+24+8+3 + bar)*2 + 1, 0x20);
    }
  }
  

  
}

		    
