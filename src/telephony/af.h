#ifndef AF_H

#define AF_H

#define AF_SMS 0
#define AF_DIALPAD 1
#define AF_CONTACT_FIRSTNAME 2
#define AF_CONTACT_LASTNAME 3
#define AF_CONTACT_PHONENUMBER 4

#define AF_ALL 0xfe
#define AF_NONE 0xff

#define AF_MAX 4

char af_retrieve(char field, char active_field, uint16_t contact_id);
char af_store(char active_field, uint16_t contact_id);
char af_redraw(char active_field, char field, uint8_t y);

#endif
