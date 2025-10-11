#ifndef CONTACTSCREENS_H

#define CONTACTSCREENS_H

char contact_draw(uint8_t x, uint8_t y,
		  uint16_t x_start_px,
		  uint8_t w_gl, uint16_t w_px,
		  unsigned int contact_id,
		  uint8_t active_field,
		  unsigned char *contact_record);

#endif
