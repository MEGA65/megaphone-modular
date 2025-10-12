#include "ascii.h"

#include "includes.h"
#include "records.h"
#include "search.h"
#include "mountstate.h"

#include <string.h>

char contact_read(uint16_t contact_id, unsigned char *buffer)
{

  try_or_fail(mount_state_set(MS_CONTACT_LIST,contact_id));
  
  try_or_fail(read_record_by_id(DRIVE_0, contact_id, buffer));

  return 0;  
}

char contact_write(uint16_t contact_id, unsigned char *buffer)
{
  try_or_fail(mount_state_set(MS_CONTACT_LIST,contact_id));

  try_or_fail(write_record_by_id(DRIVE_0, contact_id, buffer));

  return 0;  
}

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
  // Nothing to do
  try_or_fail(mount_state_set(MS_CONTACT_QSO,contact));

  return 0;
}


