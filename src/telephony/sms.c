#include "includes.h"
#include <string.h>

#include "feature_select.h"
#include "buffers.h"
#include "contacts.h"
#include "records.h"
#include "search.h"
#include "index.h"
#include "sms.h"
#include "mountstate.h"

char sms_build_message(unsigned char buffer[RECORD_DATA_SIZE],unsigned int *bytes_used,
		       unsigned char txP,
		       unsigned char *phoneNumber,
		       unsigned long bcdDate,
		       unsigned long bcdTime,
		       unsigned char *messageBody
		       )
{
  unsigned char timestamp_bin[4];

  // Reserve first two bytes for record number
  *bytes_used=2;

  // Clear buffer (will intrinsically add an end of record marker = 0x00 byte)
  lfill((unsigned long)&buffer[0],0x00,RECORD_DATA_SIZE);
  
  // +1 so strings are null-terminated for convenience.
  if (append_field(buffer,bytes_used,RECORD_DATA_SIZE, FIELD_PHONENUMBER, phoneNumber, strlen((char *)phoneNumber)+1)) fail(1);

  timestamp_bin[0]=bcdDate>>0;  timestamp_bin[1]=bcdDate>>8;
  timestamp_bin[2]=bcdDate>>16; timestamp_bin[3]=bcdDate>>24;
  if (append_field(buffer,bytes_used,RECORD_DATA_SIZE, FIELD_BCDTIME, timestamp_bin, 4)) fail(2);  

  timestamp_bin[0]=bcdTime>>0;  timestamp_bin[1]=bcdTime>>8;
  timestamp_bin[2]=bcdTime>>16; timestamp_bin[3]=bcdTime>>24;
  if (append_field(buffer,bytes_used,RECORD_DATA_SIZE, FIELD_BCDDATE, timestamp_bin, 4)) fail(3);  

  if (append_field(buffer,bytes_used,RECORD_DATA_SIZE, FIELD_BODYTEXT, messageBody, strlen((char *)messageBody)+1)) fail(4);  
  if (append_field(buffer,bytes_used,RECORD_DATA_SIZE, FIELD_MESSAGE_DIRECTION, &txP, 1)) fail(5);

  return 0;
}

char sms_log(unsigned char *phoneNumber, unsigned long bcdDate,
	     unsigned long bcdTime,
	     unsigned char *message, char direction)
{      
  // 1. Work out which contact the message is to/from
  unsigned int contact_ID = search_contact_by_phonenumber(phoneNumber);

  return sms_log_to_contact(contact_ID,phoneNumber, bcdDate, bcdTime, message, direction);
}

char sms_send_to_contact(unsigned int contact_ID, unsigned char *message)
{
  unsigned char *phoneNumber = NULL;
  unsigned long bcdDate, bcdTime;
  unsigned int len;
  unsigned char r;
  
  // Mount contact D81 to get phone number of this contact
  try_or_fail(mount_state_set(MS_CONTACT_LIST,contact_ID));

  // XXX - Assume RTC is valid
  bcdDate = mega65_bcddate();
  bcdTime = mega65_bcdtime();

  r=read_record_by_id(DRIVE_0,contact_ID,buffers.textbox.contact_record);
  if (r) fail(1);

  phoneNumber = find_field(buffers.textbox.contact_record,RECORD_DATA_SIZE,FIELD_PHONENUMBER,&len);
  if (!phoneNumber) phoneNumber=(unsigned char *)"UNKNOWN";

  return sms_log_to_contact(contact_ID,phoneNumber,bcdDate, bcdTime,
			    message,SMS_DIRECTION_TX);
  
}

char sms_log_to_contact(unsigned int contact_ID,
			unsigned char *phoneNumber,
			unsigned long bcdDate,
			unsigned long bcdTime,
			unsigned char *message, char direction)
{      
 
  unsigned int record_number = 0;

  // 2. Retreive that contact (or if no such contact, then use the "UNKNOWN NUMBERS" pseudo-contact?)
  // contact_find_by_phonenumber() will return contact 1 always
#ifdef CROSS_COMPILED
  fprintf(stderr,"DEBUG: Phone number '%s' is contact %d\n",phoneNumber,contact_ID);
#endif
  
  if (buffers_lock(LOCK_TELEPHONY)) fail(99);
  
  if (read_record_by_id(DRIVE_0, contact_ID,buffers.telephony.contact)) {
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
      write_record_by_id(DRIVE_0,contact_ID,buffers.telephony.contact);
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
  if (read_sector(DRIVE_0,1,0)) fail(3);
  record_number = record_allocate_next( SECTOR_BUFFER_ADDRESS );
  
  if (!record_number) {
    fail(4);
  } else {    
    // Write back updated BAM
    if (write_sector(DRIVE_0,1,0)) fail(5);
    mega65_uart_print("Allocated record ");
    mega65_uart_printhex16(record_number);
    mega65_uart_print(" for new SMS message\r\n");
    lfill(SECTOR_BUFFER_ADDRESS,0x00,512);
    if (read_sector(DRIVE_0,1,0)) fail(51);    
  }
#ifdef CROSS_COMPILED
  fprintf(stderr,"DEBUG: Allocated message record #%d in contact #%d\n",
	  record_number,contact_ID);
#endif
  
  // 6. Build message and store.
  if (buffers_lock(LOCK_TELEPHONY)) fail(6);
  if (read_record_by_id(DRIVE_0,record_number,buffers.telephony.message)) {
    buffers_unlock(LOCK_TELEPHONY);  
    fail(7);
  }
  if (sms_build_message(buffers.telephony.message,
			&buffers.telephony.message_bytes,			
		        direction,
			phoneNumber, bcdDate, bcdTime, message)) {
    buffers_unlock(LOCK_TELEPHONY);  
    fail(8);
  }
  sectorise_record(buffers.telephony.message, (unsigned long)&buffers.telephony.sector_buffer[0]);
  lcopy((unsigned long)buffers.telephony.sector_buffer,SECTOR_BUFFER_ADDRESS,512);
  if (write_record_by_id(DRIVE_0,record_number,buffers.telephony.message)) {
    buffers_unlock(LOCK_TELEPHONY);  
    fail(9);
  }

  // 7. Update used message count in conversation (2nd half of BAM sector?)
  // XXX - Don't need it, because we have the allocation stuff.
  // XXX - But it could still make it a little more efficient.

#ifdef MAINTAIN_THREAD_INDEX
  // 8. Update thread index for this message
  index_buffer_clear();
  index_buffer_update(message,strlen((char *)message));
  index_update_from_buffer(DRIVE_1,record_number);
#endif
  
  buffers_unlock(LOCK_TELEPHONY);    

  return 0;
}

char sms_delete_message(unsigned int contact_ID, int message_number)
{
  int record_number;

  // 1. Make sure we have the contact loaded
  if (mount_contact_qso(contact_ID)) { fail(1); return 1; }
  
  // 2. Read the BAM
  if (read_sector(DRIVE_0,1,0)) { fail(2); return 2; }

  // 3. Resolve a relative message number if necessary
  int next_record_number = record_allocate_next( SECTOR_BUFFER_ADDRESS );
  // Then get back the original version
  if (read_sector(DRIVE_0,1,0)) { fail(3); return 3; }
  if (message_number < 0 ) {
    record_number = next_record_number + message_number;
    if (record_number < 0) { fail(4); return 4; }
  } else record_number = message_number;
  if (record_number >= next_record_number) { fail(5); return 5; }
  if (!record_number) { fail(6); return 6; }
  
  // 4. Erase bits for this record in all index records.
  index_buffer_clear();
  index_buffer_update((unsigned char *)"",0);
  index_update_from_buffer(DRIVE_1,record_number);
  
  // Check if we are deleting the last message, in which case we don't
  // need to shuffle down, and can simply unindex the message.
  // But if it's not the last we have to reindex multiple records.
  // For now, in that case we'll just reindex the whole thing.
  // Actually... we can just shuffle the index bits down.

  if (record_number < (next_record_number - 1) ) {
    // It wasn't the last record, so we have to shuffle the index entries as well.
    // For now, just re-index the whole thing.
    // XXX Disabled for now because we don't currently use the index,
    //     and it takes _forever_. Like _hours_ to reindex a disk on real hardware
    //     right now.
    // XXX disk_reindex(FIELD_BODYTEXT);
  }

  // 5. Update BAM sector for thread to mark the message free
  if (read_sector(DRIVE_0,1,0)) { fail(7); return 7; }
  if (record_free(SECTOR_BUFFER_ADDRESS + 2,record_number)) { fail(9); return 9; }
  if (write_sector(DRIVE_0,1,0)) { fail(8); return 8; }
  
  buffers_unlock(LOCK_TELEPHONY);    
  
  return 0;
}


