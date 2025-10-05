#include "includes.h"
#include "records.h"
#include "slab.h"
#include "buffers.h"

// #define DEBUG_MERGE

int compare_records(unsigned char a_idx, unsigned char b_idx, unsigned char field_id) {
  unsigned int len_a = 0, len_b = 0, l;
  unsigned long addr;
  unsigned char *field_a, *field_b;
  unsigned int min_len;
  char cmp;
  
  if (b_idx != buffers.sort.idx_b) {
    // B record cache invalid so desectorise from high RAM
    addr = WORK_BUFFER_ADDRESS + (((unsigned long)b_idx)<<9);
    lcopy(addr + 2,(unsigned long)&buffers.sort.rec_b[0],254);
    lcopy(addr + 256+2,(unsigned long)&buffers.sort.rec_b[254],254);
    // And mark the record as cached
    buffers.sort.idx_b = b_idx;
  }
  
  // Always fetch record a
  addr = WORK_BUFFER_ADDRESS + (((unsigned long)a_idx)<<9);
  lcopy(addr + 2,(unsigned long)&buffers.sort.rec_a[0],254);
  lcopy(addr + 256+2,(unsigned long)&buffers.sort.rec_a[254],254);      
  
  // char *find_field(char *record, unsigned int bytes_used, unsigned char type, unsigned int *len);
  field_a = find_field(buffers.sort.rec_a, RECORD_DATA_SIZE, field_id, &len_a);
  field_b = find_field(buffers.sort.rec_b, RECORD_DATA_SIZE, field_id, &len_b);
  
  min_len = (len_a < len_b) ? len_a : len_b;
  cmp=0;
  for(l=0;l<min_len;l++) {
    if (field_a[l]<field_b[l]) { cmp=-1; break;}
    if (field_a[l]>field_b[l]) { cmp=1; break;}
  }
  
  if (cmp == 0) {
    return (int)len_a - (int)len_b; // shorter field sorts first
  }
  return cmp;
}

unsigned char quicksort_indices(unsigned char field_id, unsigned char count) {
  unsigned char low,high, pivot_idx, i, j, tmp;
  
  // Initialize stack with full range
  buffers.sort.stack[0].low = 0;
  buffers.sort.stack[0].high = count - 1;
  buffers.sort.stack_ptr = 1;
  
  while (buffers.sort.stack_ptr > 0) {
    buffers.sort.stack_ptr--;
    low  = buffers.sort.stack[buffers.sort.stack_ptr].low;
    high = buffers.sort.stack[buffers.sort.stack_ptr].high;
    
    if (low >= high) continue;
    
    // Choose pivot = high
    pivot_idx = buffers.sort.indices[high];
   i = low;
    
    for (j = low; j < high; j++) {
      if (compare_records(buffers.sort.indices[j], pivot_idx, field_id) <= 0) {
	// Swap indices[i] and indices[j]
	tmp = buffers.sort.indices[i];
	buffers.sort.indices[i] = buffers.sort.indices[j];
	buffers.sort.indices[j] = tmp;
	i++;
      }
    }
    
    // Move pivot into correct location
    tmp = buffers.sort.indices[i];
    buffers.sort.indices[i] = buffers.sort.indices[high];
    buffers.sort.indices[high] = tmp;
    
    // Push sub-partitions onto stack
    if (i > low + 1) {
      buffers.sort.stack[buffers.sort.stack_ptr].low = low;
      buffers.sort.stack[buffers.sort.stack_ptr].high = i - 1;
      buffers.sort.stack_ptr++;
    }
    if (i + 1 < high) {
      buffers.sort.stack[buffers.sort.stack_ptr].low = i + 1;
      buffers.sort.stack[buffers.sort.stack_ptr].high = high;
      buffers.sort.stack_ptr++;
    }
  }

  return 0;
}

unsigned char sort_slab(unsigned char field_id)
{
  /*
    Do a non-recursive quicksort of the 80KB slab of sectors
    from drive 0, using drive 1 as temporary store
   */
  unsigned char c;

  if (buffers_lock(LOCK_SORT)) fail(99);
  
  // Set up ordinal record numbers
  for(c=0;c<QSORT_MAX_RECORDS;c++) buffers.sort.indices[c]=c;
  // invalidate record cache for idx_b, which is repeatedly used in quicksort.
  // This saves us from copying down records that we already have buffered.
  buffers.sort.idx_b=0xff; 
  // Sort the list of indices
  quicksort_indices(field_id, QSORT_MAX_RECORDS);
  // Write the sorted indices out to the temporary D81 in drive 1

  if (buffers_unlock(LOCK_SORT)) fail(98);

  return 0;  
}

#define NUM_SLABS (80/8)
unsigned char cached_track[NUM_SLABS];
unsigned char cached_track_stop[NUM_SLABS];
unsigned char cached_next_sector[NUM_SLABS];
unsigned char next_slab, next_slab_sector;  

char sort_d81(char *name_in, char *name_out, unsigned char field_id)
{
  unsigned char out_track=1;
  unsigned char out_sector=0;
  unsigned char slab=0;
  unsigned char s;
  
  // Mount D81 to be sorted in drive 0
  if (mount_d81(name_in,0)) fail(7);
  
  // Mount a scratch D81 in drive 1  
  if (mount_d81("SCRATCH.D81",1)) fail(8);

  // fprintf(stderr,"DEBUG: sort_d81(): Sort each slab.\n");
  
  // Do internal sort of each 80KB slab of the disk.
  for(slab=0;slab<SLAB_COUNT;slab++) {

    unsigned char n=0, t=0, t_stop=0, s=0;
    
    // Read in the slab
    if (slab_read(0,slab)) fail(1);

    // Get a sorted list of indices
    if (sort_slab(field_id)) fail(8);

    // Write out the sorted sectors into the scratch disk image
    t=1+(slab<<3);
    t_stop = t+SLAB_TRACKS;
    for(;t<t_stop;t++) {
      for(s=0;s<20;s++) {
	unsigned long rec_addr
	  = WORK_BUFFER_ADDRESS + (((unsigned long)buffers.sort.indices[n])<<9);
	
	lcopy(rec_addr,(unsigned long)SECTOR_BUFFER_ADDRESS,512);

	if (write_sector(1,t,s)) fail(2);

	n++;
      }
    }
    
  }

  // Now we have the slabs sorted, we need to do an external merge of those slabs back into the 
  // sorted output D81.
  // We don't care about the sector links in the sorted disk images, because these images are
  // generated on demand. If someone validates a sorted D81, the result won't be tragic, since
  // we can just regenerate it.

  // Mount output D81 as drive 0
  if (mount_d81(name_out,0)) fail(9);

  // fprintf(stderr,"DEBUG: sort_d81(): Read merge cache.\n");
  
  // Prime cache of sectors from each slab. We have 128KB at WORK_BUFFER_ADDRESS we can use.
  // So we will make each cache be a whole track.
  for(slab=0;slab<10;slab++) {
    cached_track[slab]=(slab<<3)+1;
    cached_track_stop[slab]=(slab<<3)+1+8;
    cached_next_sector[slab]=0;

    // fprintf(stderr,"DEBUG: Reading track %d for slab %d\n",cached_track[slab],slab);
    
    for(s=0;s<20;s++) {
      if (read_sector(1,cached_track[slab],s)) fail(3);
      lcopy((unsigned long)SECTOR_BUFFER_ADDRESS,WORK_BUFFER_ADDRESS + slab*(512L*20) + s*512L, 512);
    }
  }

  // Now do round-robin comparisons of the next sector in each cached track to find
  // the next sector to write out to the disk image.
  // Stop when no more sectors to check.
  do {
    next_slab=0xff;
    next_slab_sector = 0;

    for(slab=0;slab<10;slab++) {
      if (cached_next_sector[slab]==20) continue;
      if (next_slab==0xff) {
	next_slab = slab;
	next_slab_sector = cached_next_sector[slab];
      } else {
	if (compare_records(slab*20 + cached_next_sector[slab], next_slab*20 + next_slab_sector, field_id) <= 0) {
	  next_slab = slab;
	  next_slab_sector = cached_next_sector[slab];
	}
      }
    }
    
    if (next_slab==0xff) break;

#ifdef DEBUG_MERGE
    fprintf(stderr,"DEBUG: Writing sorted record to T%d,S%d from slab=%d,T%d,S%d (stop track=%d)\n",
	    out_track,out_sector,
	    next_slab,cached_track[next_slab], cached_next_sector[next_slab],
	    cached_track_stop[next_slab]
	    );
    dump_bytes("the record",
	       WORK_BUFFER_ADDRESS + (next_slab*20L + cached_next_sector[next_slab])*512L,512);
	     
#endif

    // Copy the record we need into the sector buffer
    lcopy(WORK_BUFFER_ADDRESS + (next_slab*20L + cached_next_sector[next_slab])*512L,
	  SECTOR_BUFFER_ADDRESS,512);
    
    // Write this record to disk
    if (write_sector(0,out_track,out_sector)) fail(5);
    out_sector++;
    if (out_sector==20) {
      out_sector=0;
      out_track++;
    }
    
    // Bump next sector of the slab we've just used
    next_slab_sector++;
    if (next_slab_sector==20) {
      cached_track[next_slab]++;
      if (cached_track[next_slab]<cached_track_stop[next_slab]) {
	// Read the track
#ifdef DEBUG_MERGE
	fprintf(stderr,"DEBUG: Reading track %d for slab %d\n",cached_track[next_slab],next_slab);
#endif
	for(s=0;s<20;s++) {
	  if (read_sector(1,cached_track[next_slab],s)) fail(6);
	  lcopy((unsigned long)SECTOR_BUFFER_ADDRESS,WORK_BUFFER_ADDRESS + next_slab*(512L*20) + s*512L, 512);
	}
	next_slab_sector=0;
      }
    }
    cached_next_sector[next_slab]=next_slab_sector;
  
  } while(next_slab!=0xff);

  return 0;
}
