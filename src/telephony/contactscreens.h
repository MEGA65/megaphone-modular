#ifndef CONTACTSCREENS_H

#define CONTACTSCREENS_H

// label width must be odd to not cause weird GOTOX glitch with CHARY16 mode
#define LABEL_WIDTH_PX 47
#define LABEL_WIDTH_GL 10

char contact_draw(uint8_t x, uint8_t y,
		  uint16_t x_start_px,
		  uint8_t w_gl, uint16_t w_px,
		  uint16_t contact_id,
		  uint8_t active_field);

#endif
