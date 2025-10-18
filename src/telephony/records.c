// Include file for either MEGA65 or Linux compilation
#include "includes.h"

#include "records.h"

unsigned int record_allocate_next(unsigned long bam_sector_address)
{
  /* Find an unallocated sector on the D81 using our native allocation method,
     not the D81's DOS BAM which we always have set to show as completely full.

     We use a simple bitmap for allocation, where 1 = ALLOCATED (not free, like the D81 DOS).
     We can fit 512 x 8 = 4K entries in a single 512 byte sector.
     In fact, we can fit the entire allocation information in the first 254-byte data area
     of the physical sector, since 254x8 = 2,032, but there are only 1,680 sectors.
  */
  unsigned char i,j,b;

  // dump_bytes("Sector BAM before allocation",&bam_sector[2],USABLE_SECTORS_PER_DISK/8);
  
  for(i=0;i<(USABLE_SECTORS_PER_DISK/8);i++) {
    // Found a BAM byte that's not full?
    if (lpeek(bam_sector_address+2+i)!=0xff) {
      // Work out which bit is clear, set it, and return the allocated record.
      b = lpeek(bam_sector_address+2+i);
      for(j=0;j<8;j++) {
	if (!(b&1)) {
	  lpoke(bam_sector_address+2+i, lpeek(bam_sector_address+2+i) | (1<<j) );
	  // Make sure we're not trying to allocate sector 0 (where the BAM lives)
	  // (but note the line above means that we will implicitly mark sector 0 as allocated
	  // in the process).
	  if (i+j) {
	    // 	    dump_bytes("BAM sector after allocation",bam_sector_address,256);
	    return (i<<3)+j;
	  }
	}
	b=b>>1;
      }
    }
  }

  // BAM sector is 0, which if we return that, then we mean no sector could be allocated.
  return 0;
}

char record_free(unsigned long bam_sector,unsigned int record_num)
{
  unsigned char b, bit;
  if (record_num>=USABLE_SECTORS_PER_DISK) fail(1);
  b=lpeek(bam_sector + (record_num>>3));
  bit = 1<<(record_num&7);
  if (!(b&bit)) { fail(2); return 2; } // Record wasn't allocated
  b&=(0xff-bit);
  lpoke(bam_sector + (record_num>>3), b);
  return 0;
}


void sectorise_record(unsigned char *record,
		      unsigned long sector_buffer)
{  
  lcopy((unsigned long)&record[0],(unsigned long)sector_buffer+2,254);
  lcopy((unsigned long)&record[254],(unsigned long)sector_buffer+256+2,254);
}

void desectorise_record(unsigned long sector_buffer,
			unsigned char *record)
{
  lcopy((unsigned long)sector_buffer+2,(unsigned long)&record[0],254);
  lcopy((unsigned long)sector_buffer+256+2,(unsigned long)&record[254],254);
}

char append_field(unsigned char *record, unsigned int *bytes_used, unsigned int length,
		  unsigned char type, unsigned char *value, unsigned int value_length)
{
  // Insufficient space
  if (((*bytes_used)+1+1+value_length)>=length) fail(1);
  // Field too long
  if (value_length > 511) fail(2);
  // Type must be even
  if (type&1) fail(3);

  record[(*bytes_used)++]=type|(value_length>>8);
  record[(*bytes_used)++]=value_length&0xff;
  if (value_length) {
    lcopy((unsigned long)value,(unsigned long)&record[*bytes_used],value_length);
    (*bytes_used)+=value_length;
  }

  return 0;
}

char delete_field(unsigned char *record, unsigned int *bytes_used, unsigned char type)
{
  unsigned int ofs=2;      // Skip record number indicator
  unsigned char deleted=0;

  while(ofs<(*bytes_used)&&record[ofs]) {
    uint16_t field_len = ((record[ofs]&1)?256:0) + record[ofs+1];
    unsigned int shuffle = 1 + 1 + field_len;
    
    if ((record[ofs]&0xfe) == type) {
      // Found matching field
      // Now to shuffle it down.
      // On MEGA65, lcopy() has "special" behaviour with overlapping copies.
      // Basically the first few bytes will be read before the first byte is written.
      // The nature of the copy that we are doing, shuffling down, is safe in this context.
      if ((*bytes_used)-ofs-shuffle) {
	lcopy((unsigned long)&record[ofs+shuffle],(unsigned long)&record[ofs],
	      (*bytes_used)-ofs-shuffle);	
      } else {
	// A zero-length shuffle happens when we delete the last field in a record.
	// This is totally fine
      }
      (*bytes_used) -= shuffle;

      if (*bytes_used > RECORD_DATA_SIZE) {
	fail(2);
      }

      deleted++;
      
      // There _shouldn't_ be multiple fields with the same type in a record, but who knows?
      // So we'll just continue and look for any others.
    } else ofs+=shuffle;
  }
  return deleted;
}

unsigned char *find_field(unsigned char *record, unsigned int bytes_used, unsigned char type, unsigned int *len)
{
  unsigned int ofs=2;  // Skip record number indicator

  while((ofs<=bytes_used)&&record[ofs]) {
    unsigned int shuffle = 1 + 1 + ((record[ofs]&1)?256:0) + record[ofs+1];

    if ((record[ofs]&0xfe) == type) {
      if (len) *len = ((record[ofs]&1)?256:0) + record[ofs+1];
      return &record[ofs+2];
    }

    ofs+=shuffle;    
  }
  return NULL;
}

unsigned int record_get_bytes_used(unsigned char *record)
{
  unsigned int ofs=2;  // Skip record number indicator

  while((ofs<=RECORD_DATA_SIZE)&&record[ofs]) {    
    unsigned int shuffle = 1 + 1 + ((record[ofs]&1)?256:0) + record[ofs+1];
    
    if (!record[ofs]) break;
    
    ofs+=shuffle;

    if (ofs>=RECORD_DATA_SIZE) {
      fail(1);
    }
  }
  return ofs;
}

char read_record_by_id(unsigned char drive_id,unsigned int id,unsigned char *buffer)
{
  // The ID let's us determine the track and sector.

  unsigned char track, sector;

  // Work out where the sector gets written to
  track=1 + id/20;  // Track numbers start at 1
  sector=id - ((track-1)*20);
  if (track>39) track++;
  
  // And ID of 0 is invalid (it's the record BAM sector).
  if (!id) fail(1);

  if (read_sector(drive_id,track,sector)) fail(3);

  desectorise_record(SECTOR_BUFFER_ADDRESS,buffer);
  
  return 0;
}

char write_record_by_id(unsigned char drive_id,unsigned int id,unsigned char *buffer)
{
  // The ID let's us determine the track and sector.

  unsigned char track, sector;

  // Work out where the sector gets written to
  track=1 + id/20;  // Track numbers start at 1
  sector=id - ((track-1)*20);
  if (track>39) track++;

  // And ID of 0 is invalid (it's the record BAM sector).
  if (!id) fail(1);

  // fprintf(stderr,"DEBUG: Trying to write contact #%d\n",id);

  // Read original sector in to get inter-sector links
  if (read_sector(drive_id,track,sector)) fail(4);

  // Then write the contact body over the data area
  sectorise_record(buffer,SECTOR_BUFFER_ADDRESS);

  // dump_bytes("Sectorised Contact",(unsigned char *)SECTOR_BUFFER_ADDRESS,512);

  // And commit it back to disk
  // fprintf(stderr,"DEBUG: write_sector(%d,%d,%d)\n",drive_id,track,sector);
  if (write_sector(drive_id,track,sector)) fail(3);

  return 0;
}

