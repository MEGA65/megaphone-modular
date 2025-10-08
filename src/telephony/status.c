#include "includes.h"

#include "screen.h"
#include "status.h"

/*
  Status bar structure:

  0-63px (glyphs 0--15) = time

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
  statusbar_draw_time();
}

		    
