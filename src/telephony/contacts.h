#ifndef CONTACTS_H
#define CONTACTS_H

char build_contact(unsigned char buffer[508],unsigned int *bytes_used,
		   unsigned char *firstName,
		   unsigned char *lastName,
		   unsigned char *phoneNumber,
		   unsigned int unreadCount);
char mount_contact_qso(unsigned int contact);
char contact_read(uint16_t contact_id, unsigned char *buffer);
char contact_write(uint16_t contact_id, unsigned char *buffer);

#endif
