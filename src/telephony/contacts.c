#include "ascii.h"

#include "includes.h"
#include "records.h"
#include "search.h"

#include <string.h>

char build_contact(unsigned char buffer[RECORD_DATA_SIZE],unsigned int *bytes_used,
		   unsigned char *firstName,
		   unsigned char *lastName,
		   unsigned char *phoneNumber,
		   unsigned int unreadCount)
{
  unsigned char urC[2];
  
  // Reserve first two bytes for record number
  *bytes_used=2;

  urC[0]=unreadCount&0xff;
  urC[1]=unreadCount>>8;
  
  // Clear buffer (will intrinsically add an end of record marker = 0x00 byte)
  lfill((unsigned long)&buffer[0],0x00,RECORD_DATA_SIZE);
  
  // +1 so strings are null-terminated for convenience.
  if (append_field(buffer,bytes_used,RECORD_DATA_SIZE, FIELD_FIRSTNAME, firstName, strlen((char *)firstName)+1)) fail(1);
  if (append_field(buffer,bytes_used,RECORD_DATA_SIZE, FIELD_LASTNAME, lastName, strlen((char *)lastName)+1)) fail(2);
  if (append_field(buffer,bytes_used,RECORD_DATA_SIZE, FIELD_PHONENUMBER, phoneNumber, strlen((char *)phoneNumber)+1)) fail(3);
  if (append_field(buffer,bytes_used,RECORD_DATA_SIZE, FIELD_UNREAD_MESSAGES, urC, 2)) fail(4);

  return 0;
}

char mount_contact_qso(unsigned int contact)
{
  unsigned char r;
  char hex[2];
  if (mega65_cdroot()) fail(9);
  if (mega65_chdir("PHONE")) fail(1);
  if (mega65_chdir("THREADS")) fail(2);
  hex[0]=to_hex(contact>>12);
  hex[0]='?';
  hex[1]=0;
  if (mega65_chdir(hex)) fail(3);
  hex[0]=to_hex(contact>>8);
  hex[1]=0;
  if (mega65_chdir(hex)) fail(4);
  hex[0]=to_hex(contact>>4);
  hex[1]=0;
  if (mega65_chdir(hex)) fail(5);
  hex[0]=to_hex(contact>>0);
  hex[1]=0;
  if (mega65_chdir(hex)) fail(6);

  if ((r=mount_d81("MESSAGES.D81",0))!=0) {
    lpoke(0x12003L,r);
    fail(10);
  }
  if (mount_d81("MSGINDEX.D81",1)) fail(8);

  return 0;
}
