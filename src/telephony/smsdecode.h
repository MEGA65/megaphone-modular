typedef struct {
  char sender[32];      // "+614..." etc
  char text[512];       // utf-8 output (size to taste)
  int  text_len;
  
  // Optional metadata
  uint8_t dcs;
  uint8_t udhi;
  
  // Concatenation (if UDH has it)
  uint8_t concat;
  uint16_t concat_ref;
  uint8_t  concat_total;
  uint8_t  concat_seq;
} sms_decoded_t;

int decode_sms_deliver_pdu(const char *pdu_hex, sms_decoded_t *s);
