#include "includes.h"
#include "records.h"
#include "slab.h"
#include "buffers.h"
#include "contacts.h"

// 1 bit per diphthong. We use a mod 56 number space which
// results in 56x56 = 3,136 values. One index per logical sector.
// So we can have 2xUSABLE_SECTORS_PER_DISK entries, which works
// with our 3,136 value.
#define MODULO_SPACE 56
unsigned char index_bitmap[(USABLE_SECTORS_PER_DISK<<1)>>3];
unsigned char index_bitmap_last_val = 0;

// To make indices and thus search case insensitive, we use a character
// mapping that folds case. It also means we can fold everything else
// however we like.  In particular we want to keep punctuation and
// characters/numbers from aliasing.
unsigned char index_mapping_table[256] = {
  //  0x00–0x0F
   0,  1,  2,  3,  4,  5,  6,  7,  8, 32, 32, 11, 12, 32, 14, 15, // NUL,SOH,...,\t,\n,VT,\r,...
  //  0x10–0x1F
  16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, // DC1–US
  //  0x20–0x2F
  32, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, // ' ',!,",#,$,%,&,',(,),*,+,,,,-,.,/
  //  0x30–0x3F
  42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 27, 28, // 0–9,: ,;,<,=,>,?
  //  0x40–0x4F
  0,   1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, // @, A–0
  //  0x50–0x5F
  16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 32, 30, 31, 33, 34, // P–Z,[,\,,],^,_
  //  0x60–0x6F
  35,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, // `,a–p
  //  0x70–0x7F
  16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 38, 39, 40, 41, 42, // q–z,{,|,},~,DEL
  //  0x80–0x8F
  43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55,  0,  1,  2, // Extended
  //  0x90–0x9F
   3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, //
  //  0xA0–0xAF
  19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, //
  //  0xB0–0xBF
  35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, //
  //  0xC0–0xCF
  51, 52, 53, 54, 55,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, //
  //  0xD0–0xDF
  11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, //
  //  0xE0–0xEF
  27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, //
  //  0xF0–0xFF
  43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55,  0,  1,  2  //
};

void index_buffer_clear(void)
{
  lfill((unsigned long)&index_bitmap[0],0x00,sizeof(index_bitmap));
  index_bitmap_last_val = 0;

  return;
}

void index_buffer_update(unsigned char *d,unsigned int len)
{
  while(len) {
    unsigned int diphthong = index_mapping_table[*d];
    diphthong += 56 * index_bitmap_last_val;
    index_bitmap_last_val = index_mapping_table[*d];

    if (diphthong >= (56*56)) {
#ifdef CROSS_COMPILED
      fprintf(stderr,"FATAL: Illegal diphthong 0x%04x. Should not be possible.\n",
	      diphthong);
      exit(-1);
#else
      // Just fail silently on MEGA65. Not ideal. But it really shouldn't be possible
      return;
#endif
    }
    
    // Don't index diphthongs where both characters == 0x00
    // (so that we don't make hits on unsused bytes in records)
    if (*d) {
      // fprintf(stderr,"DEBUG: Diphthong 0x%03x is in the indexed text.\n",
      //         diphthong);
      index_bitmap[diphthong>>3] |= 1<<(diphthong&0x07);
    }
    
    d++; len--;
  }
  return;
}

/* This is where the real fun is:
   Open the D81, and read in blocks of WORK_BUFFER_SIZE, and
   set or clear the bit for this message, and then write the whole slab
   back after.  This balances efficiency with memory usage.
*/

#define INDEX_PAGES_PER_SLAB (SLAB_TRACKS*20*2)

char index_update_from_buffer(unsigned char disk_id, unsigned int record_number)
{
  unsigned int index_page = 0, ofs;
  unsigned char slab;

  mega65_uart_print("Indexing disk ");
  mega65_uart_printhex(disk_id);
  mega65_uart_print("\r\n");
  
  // Calculate which byte and bit we are fiddling in each index page
  // +2 to skip the track/sector 
  unsigned char record_byte = 2 + (record_number>>3);
  unsigned char record_bit = 1<<(record_number&7);

#if 0
  fprintf(stderr,"DEBUG: record_number %d -> byte=%d, bit=%d\n",
	  record_number, record_byte, record_bit);
#endif

  // For each slab update the index pages
  for(slab=0;slab<SLAB_COUNT;slab++) {
    unsigned long index_page_address = WORK_BUFFER_ADDRESS;
  
    // Read a slab
    if (slab_read(disk_id, slab)) fail(2);
    
    for(ofs=0;ofs<INDEX_PAGES_PER_SLAB;ofs++) {

      // Find the diphthong byte and bit in buffers.index.rec that corresponds
      // to the index page.
      unsigned int byte = index_page>>3;
      unsigned char bit = index_bitmap[byte] & (1<<(index_page&7));

      // Retrieve the byte that for this record from this index page
      unsigned char value = lpeek(index_page_address + record_byte);

      // Do we need to set or clear the bit?
      if (bit) {
	// Set the bit in the page
	value |= record_bit;
#if 0
	fprintf(stderr,"DEBUG: Setting bit for diphthong 0x%04x (0x%04x) from record %d at slab offset 0x%05llx, slab %d\n",
		index_page,
		byte,
		record_number,
		index_page_address - WORK_BUFFER_ADDRESS,
		slab);
#endif
      } else {
	// Clear the bit in the page
	value &= (0xff - record_bit);
      }

      // Write the updated index page byte for this record
      lpoke(index_page_address + record_byte,value);

      index_page++;
      index_page_address += 0x100;
    }

    // Write the slab back with the changes
    if (slab_write(disk_id, slab)) fail(3);
    
  }

  return 0;
}

/*
  Re-index all records in disk 0 in the index in disk 1
*/
char disk_reindex(unsigned char field)
{
  unsigned int c;

  CHECKPOINT("disk_reindex() takes >1 hour to run!");
  fail(1);
  return 1;
  
  /* Note that we can't use slab_read() for the records we are
     indexing, because that gets used in index_update_from_buffer().

     This is unfortunate, because it means that we do a complete read and
     write of the index D81 _for every record in the D81 we are indexing_.

     This would take _hours_ on a MEGA65 with it's ~1MB/sec SD card interface
     and insufficient RAM for caching whole D81s (due to lack of Attic RAM on
     the Trenz modules that we are using).
   */

  unsigned char field_low=field;
  unsigned char field_high=field;

  // Make it easy to create a single index that contains all contact fields
  if (field==0xff) {
    field_low=FIELD_FIRSTNAME;
    field_high=FIELD_PHONENUMBER;
  }
  
  // For each record in the disk.
  // (remember we are 1-relative, as record 0 = record BAM)  
  for(c=1;c<=USABLE_SECTORS_PER_DISK;c++) {
    
    // Read the record
    if (read_record_by_id(0,c,buffers.index.rec)) fail(1);

    index_buffer_clear();
    
    for(field=field_low;field<=field_high;field+=2) {
      // Extract the field
      unsigned int fieldlen = 0;
      unsigned char *fieldvalue
	= find_field(buffers.index.rec, RECORD_DATA_SIZE, field, &fieldlen);
      
      // Build bitmap of all diphthongs in field
      if (fieldvalue&&fieldlen) index_buffer_update(fieldvalue, fieldlen);
    }
    
    // Update all index pages accordingly
    index_update_from_buffer(1,c);
  }
  
  return 0;
}

char contact_d81_name[13]="CONTACT0.D81";
char   index_d81_name[13]="IDX00-0.D81";

char contacts_reindex(unsigned char contacts_disk_id)
{
  unsigned char field;
  contact_d81_name[7]='0'+contacts_disk_id;
  
  if (mega65_cdroot()) fail(1);
  if (mega65_chdir("PHONE")) fail(2);

  if (mount_d81(contact_d81_name,0)) fail(3);

  for(field=FIELD_FIRSTNAME;field<=FIELD_PHONENUMBER;field+=2) {
    // Get index D81 as disk 1
    index_d81_name[6]='0'+contacts_disk_id;
    index_d81_name[3]=to_hex(field>>4);
    index_d81_name[4]=to_hex(field&0xf);
    if (mount_d81(index_d81_name,1)) fail(4);

    if (disk_reindex(field)) fail(5);
  }

  // And update the all fields index for the contacts
  if (mount_d81("IDXALL-0.D81",1)) fail(6);
  if (disk_reindex(0xff)) fail(7);
  
  return 0;
}

