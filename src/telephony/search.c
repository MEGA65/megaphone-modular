#include "includes.h"

#include <string.h>

#include "buffers.h"
#include "index.h"
#include "search.h"

/*
  Reset search of a records D81.
  The search result structure will contain all allocated records
  in positive position order.

  Requires that the disk being searched is loaded as disk 0, and the
  index is loaded as disk 1.
*/
char search_query_init(void)
{
  // Shared data structures locked by another sub-system
  if (buffers.lock&&buffers.lock != LOCK_SEARCH) fail(99);

  buffers.lock = LOCK_SEARCH;
  
  // Reset all scores to zero
  lfill((unsigned long)&buffers.search.all_scores,0x00, USABLE_SECTORS_PER_DISK);

  // Reset query (used so that we can delete diphthongs from the query.
  buffers.search.query_length = 0;
  
  // Set score of all valid records to 1, so that we start with an unfiltered
  // view, but that ignores unallocated records.

  // Read record BAM of the disk being searched
  if (read_sector(0,1,0)) fail(2);  
  lcopy(SECTOR_BUFFER_ADDRESS,(unsigned long)buffers.search.sector_buffer,512);
  
  // Transcribe bits into scores  
  buffers.search.score_overflow=0;
  buffers.search.byte=2; // skip over track/sector pointer
  buffers.search.bit=1; // skip record 0, which is record BAM
  for(buffers.search.r=1;buffers.search.r<USABLE_SECTORS_PER_DISK;
      buffers.search.r++) {
    if (buffers.search.sector_buffer[buffers.search.byte]
	&(1<<buffers.search.bit)) {
      buffers.search.all_scores[buffers.search.r]=1;
    }
    buffers.search.bit++;
    if (buffers.search.bit==8) {
      buffers.search.byte++;
      buffers.search.bit=0;
    }
  }

  buffers.search.results_stale = 0;
  buffers.search.result_count = 0;
  
  return 0;
}

char search_query_release(void)
{
  // Shared data structures locked by another sub-system
  if (buffers.lock&&buffers.lock != LOCK_SEARCH) fail(99);

  buffers.lock = LOCK_FREE;

  return 0;
}

#define BAD_DIPHTHONG 0x7fffL
unsigned int search_get_diphthong(unsigned int offset)
{
  if (offset>=(buffers.search.query_length-1)) {
#ifdef CROSS_COMPILED
    log_error_(__FILE__,__FUNCTION__,__LINE__,100);
#endif
    return BAD_DIPHTHONG;
  }

  return index_mapping_table[buffers.search.query[offset]] * 56
    + index_mapping_table[buffers.search.query[offset+1]];
}

char search_query_add_diphthong_score(unsigned int offset)
{
  unsigned int diphthong = search_get_diphthong(offset);

  if (diphthong==BAD_DIPHTHONG) fail(2);
  
  // fprintf(stderr,"DEBUG: Searching for diphthong %d\n",diphthong);
  
  // Read the index page: Remember that there are two index pages per physical
  // sector, so we shift diphthong right one bit.
  if (read_record_by_id(1,diphthong>>1,buffers.search.sector_buffer)) fail(1);
  
  // Is the index page in the lower or upper half of the physical sector?
  buffers.search.index_page_offset = 2 + ((diphthong&1)?0x100:0);

  // XXX - We could save some cycles (maybe) by only copying the 210 bytes we need
  lcopy(SECTOR_BUFFER_ADDRESS,(unsigned long) &buffers.search.sector_buffer, 512);  
  
  // Transcribe bits into scores  
  buffers.search.byte=0;
  buffers.search.bit=1;
  for(buffers.search.r=1;buffers.search.r<USABLE_SECTORS_PER_DISK;
      buffers.search.r++) {
    if (buffers.search.sector_buffer[buffers.search.index_page_offset
				     + buffers.search.byte]
	&(1<<buffers.search.bit)) {
      // Don't overflow scores.
      // This DOES create a problem when deleting, so remember that it's
      // happened, so that if we delete chars from the query, we know to
      // recalculate the scores from scratch.
      if (buffers.search.all_scores[buffers.search.r]!=0xff)
	buffers.search.all_scores[buffers.search.r]++;
      else
	buffers.search.score_overflow=1;
    }
    buffers.search.bit++;
    if (buffers.search.bit==8) {
      buffers.search.byte++;
      buffers.search.bit=0;
    }
  }

  buffers.search.results_stale = 1;

  return 0;
}

char search_query_sub_diphthong_score(unsigned int offset)
{
  unsigned int diphthong = search_get_diphthong(offset);

  if (diphthong==BAD_DIPHTHONG) fail(2);

  // Read the index page: Remember that there are two index pages per physical
  // sector, so we shift diphthong right one bit.
  if (read_record_by_id(1,diphthong>>1,buffers.search.sector_buffer)) fail(1);

  // Is the index page in the lower or upper half of the physical sector?
  buffers.search.index_page_offset = 2 + ((diphthong&1)?0x100:0);
  
  // Transcribe bits into scores  
  buffers.search.byte=0;
  buffers.search.bit=1;
  for(buffers.search.r=1;buffers.search.r<USABLE_SECTORS_PER_DISK;
      buffers.search.r++) {
    if (buffers.search.sector_buffer[buffers.search.index_page_offset
				     + buffers.search.byte]
	&(1<<buffers.search.bit)) {

      // If a score had previously saturated, and we are now subtracting from it,
      // then we can no longer trust the resulting scores.  Take note, so that we
      // know later if we need to recalculate the scores.
      if (buffers.search.all_scores[buffers.search.r]==0xff)
	buffers.search.score_recalculation_required=1;

      buffers.search.all_scores[buffers.search.r]--;
    }
    buffers.search.bit++;
    if (buffers.search.bit==8) {
      buffers.search.byte++;
      buffers.search.bit=0;
    }
  }

  buffers.search.results_stale = 1;
  
  return 0;
}

char search_query_append_string(unsigned char *c)
{
  while(*c) {
    if (search_query_append(*c)) fail(1);
    c++;
  }
  return 0;
}


char search_query_append(unsigned char c)
{
  // Shared data structures locked by another sub-system,
  // or 
  if (buffers.lock != LOCK_SEARCH) fail(99);

  // Query too long?
  if (buffers.search.query_length >= SEARCH_MAX_QUERY_LENGTH) fail(1);

  // Append character
  buffers.search.query[buffers.search.query_length++]=c;

  // Nothing to do yet if only one char, as our index is based on diphthongs
  if (buffers.search.query_length==1) return 0;

  return search_query_add_diphthong_score(buffers.search.query_length-2);
}

char search_query_rerun(void)
{
  // Cheeky way to efficiently re-run the query
  // (relies on query_init() only resetting the length, not
  // clearing the actual query string from the buffer.
  unsigned int query_len = buffers.search.query_length;
  unsigned int o;
  
  search_query_init();
  
  for(o=0;o<query_len;o++) {
    if (search_query_append(buffers.search.query[o])) fail(1);
  }
  
  return 0;
}

// Remove last character from the query
char search_query_delete_char(void)
{
  if (buffers.lock != LOCK_SEARCH) fail(99);

  // Nothing to do
  if (buffers.search.query_length<2) return 0;

  if (search_query_sub_diphthong_score(buffers.search.query_length-2)) fail(1);

  buffers.search.query_length--;

  return 0;
}

char search_query_delete_range(unsigned int first, unsigned int last)
{
  if (buffers.lock != LOCK_SEARCH) fail(99);

  if (first>=buffers.search.query_length) fail(1);
  if (last>=buffers.search.query_length) fail(2);

  // Subtract scores for all diphthongs that we are removing
  for(buffers.search.r=first-1;
      buffers.search.r<=last;
      buffers.search.r++) {
    if (buffers.search.r<=(buffers.search.query_length-2))
      search_query_sub_diphthong_score(buffers.search.r);    
  }
  // If last is not the end, then add score for the diphthong created at the junction.
  // But this is easiest done _after_ we have excised the deleted range.

  // Get cut geometry
  buffers.search.cut_len = (last-first+1);
  buffers.search.l = buffers.search.query_length - buffers.search.cut_len;
  // Perform cut
  for(buffers.search.r=first;
      buffers.search.r<=buffers.search.l;
      buffers.search.r++) {
    buffers.search.query[buffers.search.r]
      =buffers.search.query[buffers.search.r + buffers.search.cut_len];
  }
  // Update length
  buffers.search.query_length -= buffers.search.cut_len;

  // The diphthong formed by the join is now at `first', unless that is now the end
  // of the query, in which case there's nothing to do.
  if (first < buffers.search.query_length)
    return search_query_add_diphthong_score(first);        
  
  return 0;
}

char search_collate(char min_score)
{
  buffers.search.result_count=0;

  buffers.search.last_score_threshold = min_score;
  
  if (buffers.search.results_stale||buffers.search.score_recalculation_required) {
    // Reapply the whole query
    search_query_rerun();
  }
  
  for(buffers.search.r=0;buffers.search.r<USABLE_SECTORS_PER_DISK;buffers.search.r++)
    {
      if (buffers.search.all_scores[buffers.search.r]>=min_score) {
	buffers.search.scores[buffers.search.result_count]=buffers.search.all_scores[buffers.search.r];
	buffers.search.record_numbers[buffers.search.result_count++]=buffers.search.r;
      }
    }
  buffers.search.results_stale = 0;

  return 0;
}

void search_swap_results(unsigned int i, unsigned int j)
{
  unsigned char s;
  unsigned int r;
  r=buffers.search.record_numbers[i];
  s=buffers.search.scores[i];
  buffers.search.record_numbers[i] = buffers.search.record_numbers[j];
  buffers.search.scores[i] = buffers.search.scores[j];
  buffers.search.record_numbers[j] = r;
  buffers.search.scores[j] = s;
  return;
}

char search_sort_results_by_score(void)
{
  unsigned char low, high, pivot_val,i, j;
  
  if (buffers.search.results_stale||buffers.search.score_recalculation_required) {
    // Reapply the whole query
    search_query_rerun();
    search_collate(buffers.search.last_score_threshold);
  }

  // Initialize stack with full range
  buffers.search.stack[0].low = 0;
  buffers.search.stack[0].high = buffers.search.result_count - 1;
  buffers.search.stack_ptr = 1;
  
  while (buffers.search.stack_ptr > 0) {
    buffers.search.stack_ptr--;
    low  = buffers.search.stack[buffers.search.stack_ptr].low;
    high = buffers.search.stack[buffers.search.stack_ptr].high;
    
    if (low >= high) continue;
    
    // Choose pivot = high
    pivot_val = buffers.search.scores[high];
    i = low;
    
    for (j = low; j < high; j++) {
      if (buffers.search.scores[j]>pivot_val) {
	// Swap record numbers & scores
	search_swap_results(i,j);
	i++;
      }
    }
    
    // Move pivot into correct location
    search_swap_results(i,high);
    
    // Push sub-partitions onto stack
    if (i > low + 1) {
      buffers.search.stack[buffers.search.stack_ptr].low = low;
      buffers.search.stack[buffers.search.stack_ptr].high = i - 1;
      buffers.search.stack_ptr++;
    }
    if (i + 1 < high) {
      buffers.search.stack[buffers.search.stack_ptr].low = i + 1;
      buffers.search.stack[buffers.search.stack_ptr].high = high;
      buffers.search.stack_ptr++;
    }
  }

  return 0;
}

unsigned int search_contact_by_phonenumber(unsigned char *phoneNumber)
{
  unsigned int recordPhoneNumberLen;
  unsigned char *recordPhoneNumber;
  
  // Search for the phone number in contacts
  mega65_cdroot();
  mega65_chdir("PHONE");
  mount_d81("CONTACT0.D81",0);
  mount_d81("IDX06-0.D81",1);

  // Perform the search
  // We require matches to have a score equal to the phone number or higher,
  // as no string can have a lower score and be identical to the phone number
  search_query_init();
  search_query_append_string(phoneNumber);
  search_collate(strlen((char *)phoneNumber));
    
  // Search for exact match among the phone numbers
  // (we don't need to sort the records, at this point, since a wrong number with
  // repeated matching digits could yield a higher score than the actual correct
  // phone number).
  for(buffers.search.r=0;buffers.search.r<buffers.search.result_count;
      buffers.search.r++) {
    // Retrieve the record
    // fprintf(stderr,"DEBUG: '%s' might belong to contact #%d\n",
    // 	    phoneNumber,
    //	    buffers.search.record_numbers[buffers.search.r]);
    if (read_record_by_id(0, buffers.search.record_numbers[buffers.search.r],
			  buffers.search.sector_buffer)) continue;

    // Get the phone number field.
    // XXX - Support multiple phone numbers by checking each FIELD_PHONENUMBER
    
    recordPhoneNumberLen = 0;
    recordPhoneNumber = find_field(buffers.search.sector_buffer,
						  RECORD_DATA_SIZE,
						  FIELD_PHONENUMBER,
						  &recordPhoneNumberLen);
    if (phoneNumber) {
      // fprintf(stderr,"DEBUG: Comparing search phone number '%s' with '%s'\n",
      //         phoneNumber,recordPhoneNumber);
      if (!strcmp((char *)phoneNumber,(char *)recordPhoneNumber)) {
	search_query_release();
	return buffers.search.record_numbers[buffers.search.r];
      }
    }
  }  
  
  // Release shared data structures from search mode
  search_query_release();

  // No matching record, so return the well-known UNKNOWN pseudo contact which
  // is always in record 1
  return 1;
}
