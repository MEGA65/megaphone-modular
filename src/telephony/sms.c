#include "includes.h"
#include <string.h>

#include "buffers.h"
#include "contacts.h"
#include "records.h"
#include "search.h"
#include "index.h"

char sms_build_message(unsigned char buffer[RECORD_DATA_SIZE],unsigned int *bytes_used,
		       unsigned char txP,
		       unsigned char *phoneNumber,
		       unsigned long timestampAztecTime,
		       unsigned char *messageBody
		       )
{
  unsigned char timestamp_bin[4];

  // Reserve first two bytes for record number
  *bytes_used=2;

  timestamp_bin[0]=timestampAztecTime>>0;
  timestamp_bin[1]=timestampAztecTime>>8;
  timestamp_bin[2]=timestampAztecTime>>16;
  timestamp_bin[3]=timestampAztecTime>>24;
  
  // Clear buffer (will intrinsically add an end of record marker = 0x00 byte)
  lfill((unsigned long)&buffer[0],0x00,RECORD_DATA_SIZE);
  
  // +1 so strings are null-terminated for convenience.
  if (append_field(buffer,bytes_used,RECORD_DATA_SIZE, FIELD_PHONENUMBER, phoneNumber, strlen((char *)phoneNumber)+1)) fail(1);
  if (append_field(buffer,bytes_used,RECORD_DATA_SIZE, FIELD_TIMESTAMP, timestamp_bin, 4)) fail(2);  
  if (append_field(buffer,bytes_used,RECORD_DATA_SIZE, FIELD_BODYTEXT, messageBody, strlen((char *)messageBody)+1)) fail(3);  
  if (append_field(buffer,bytes_used,RECORD_DATA_SIZE, FIELD_MESSAGE_DIRECTION, &txP, 1)) fail(4);

  return 0;
}

char sms_log(unsigned char *phoneNumber, unsigned int timestampAztecTime,
	     unsigned char *message, char direction)
{      
  // 1. Work out which contact the message is to/from
  unsigned int contact_ID = search_contact_by_phonenumber(phoneNumber);
  unsigned int record_number = 0;
  
  // 2. Retreive that contact (or if no such contact, then use the "UNKNOWN NUMBERS" pseudo-contact?)
  // contact_find_by_phonenumber() will return contact 1 always
#ifdef CROSS_COMPILED
  fprintf(stderr,"DEBUG: Phone number '%s' is contact %d\n",phoneNumber,contact_ID);
#endif
  
  if (buffers_lock(LOCK_TELEPHONY)) fail(99);
  
  if (read_record_by_id(0, contact_ID,buffers.telephony.contact)) {
    buffers_unlock(LOCK_TELEPHONY);
    fail(1);
  }
			
  // 3. Increase unread message count by 1, and write back.
  if (direction==SMS_DIRECTION_RX) {
    unsigned int unreadMessageCountLen = 0;
    unsigned char *unreadMessageCount
      = find_field(buffers.telephony.contact,RECORD_DATA_SIZE,FIELD_UNREAD_MESSAGES,
		   &unreadMessageCountLen);
    if (unreadMessageCountLen == 2) {
      unreadMessageCount[0]++;
      if (!unreadMessageCount[0]) {
	unreadMessageCount[1]++;
	if (!unreadMessageCount[1]) {
	  // Saturate rather than wrap at 64K unread messages
	  unreadMessageCount[0]=0xff;
	  unreadMessageCount[1]=0xff;
	}
      } 
      write_record_by_id(0,contact_ID,buffers.telephony.contact);
    } else {
      // No unread message count in the contact. Silently ignore for now.
    }
  }
  
  buffers_unlock(LOCK_TELEPHONY);
  
  // 4. Obtain contact physical ID from contact record, and then find and open the message D81 for that conversation.
  // Actually, the way we do the indexes on the unsorted contact D81 means that contact_ID
  // should already be the physical location of the contact in that D81.
  // So we just need to do the path magic and them mount it, and the message body index for it.
  // mount_contact_qso() mounts MESSAGES.D81 as disk 0, and MSGINDEX.D81 as disk 1
  if (mount_contact_qso(contact_ID)) fail(2);
  
  // 5. Allocate message record in conversation
  if (read_sector(0,1,0)) fail(3);
  record_number = record_allocate_next( (unsigned char *)SECTOR_BUFFER_ADDRESS );
  if (!record_number) {
    fail(4);
  } else {    
    // Write back updated BAM
    if (write_sector(0,1,0)) fail(5);
  }
#ifdef CROSS_COMPILED
  fprintf(stderr,"DEBUG: Allocated message record #%d in contact #%d\n",
	  record_number,contact_ID);
#endif
  
  // 6. Build message and store.
  if (buffers_lock(LOCK_TELEPHONY)) fail(6);
  if (read_record_by_id(0,record_number,buffers.telephony.message)) {
    buffers_unlock(LOCK_TELEPHONY);  
    fail(7);
  }
  if (sms_build_message(buffers.telephony.message,
			&buffers.telephony.message_bytes,			
		        direction,
			phoneNumber, timestampAztecTime, message)) {
    buffers_unlock(LOCK_TELEPHONY);  
    fail(8);
  }
  sectorise_record(buffers.telephony.message, buffers.telephony.sector_buffer);
  lcopy((unsigned long)buffers.telephony.sector_buffer,SECTOR_BUFFER_ADDRESS,512);
  if (write_record_by_id(0,record_number,buffers.telephony.message)) {
    buffers_unlock(LOCK_TELEPHONY);  
    fail(9);
  }

  // 7. Update used message count in conversation (2nd half of BAM sector?)
  // XXX - Don't need it, because we have the allocation stuff.
  // XXX - But it could still make it a little more efficient.
  
  // 8. Update thread index for this message
  index_buffer_clear();
  index_buffer_update(message,strlen((char *)message));
  index_update_from_buffer(1,record_number);
  
  buffers_unlock(LOCK_TELEPHONY);    
  return 0;
}
