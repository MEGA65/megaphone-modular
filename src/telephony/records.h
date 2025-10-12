#ifndef RECORDS_H
#define RECORDS_H

#define USABLE_SECTORS_PER_DISK (79*20-1)

// Data records fir 512 - 2x CBM DOS track,sector links
#define RECORD_DATA_SIZE (512 - 2 - 2)

// Fields used for contacts
#define FIELD_FIRSTNAME 0x02
#define FIELD_LASTNAME 0x04
#define FIELD_PHONENUMBER 0x06
// #define FIELD_QSODISKIMAGE 0x08  Now handled via the first 2 data bytes being the unchanging record number
#define FIELD_UNREAD_MESSAGES 0x0a

// Fields used for messages
#define FIELD_MESSAGE_DIRECTION 0x0c
// #define FIELD_TIMESTAMP 0x0e Replaced by BCD fields below
#define FIELD_BODYTEXT 0x10
#define FIELD_BCDDATE 0x12
#define FIELD_BCDTIME 0x14

#define SMS_DIRECTION_RX 0x00
#define SMS_DIRECTION_TX 0x01

#define DRIVE_0 0
#define DRIVE_1 1

unsigned int record_allocate_next(unsigned long bam_sector_address);
char record_free(unsigned long bam_sector_Address,unsigned int record_num);

char write_record_by_id(unsigned char drive_id,unsigned int id,unsigned char *buffer);
char read_record_by_id(unsigned char drive_id,unsigned int id,unsigned char *buffer);

void sectorise_record(unsigned char *record,
		      unsigned long sector_buffer);
void desectorise_record(unsigned long sector_buffer,
			unsigned char *record);

char append_field(unsigned char *record, unsigned int *bytes_used, unsigned int length,
		  unsigned char type, unsigned char *value, unsigned int value_length);
char delete_field(unsigned char *record, unsigned int *bytes_used, unsigned char type);
unsigned char *find_field(unsigned char *record, unsigned int bytes_used, unsigned char type, unsigned int *len);
unsigned int record_get_bytes_used(unsigned char *record);

#endif
