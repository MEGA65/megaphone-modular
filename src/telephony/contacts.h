#ifndef CONTACTS_H
#define CONTACTS_H

char build_contact(unsigned char buffer[508],unsigned int *bytes_used,
		   unsigned char *firstName,
		   unsigned char *lastName,
		   unsigned char *phoneNumber,
		   unsigned int unreadCount);

#endif
