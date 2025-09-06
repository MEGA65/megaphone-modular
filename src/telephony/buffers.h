#ifndef BUFFERS_H
#define BUFFERS_H

#include "records.h"

// We sort 160 records = 80KB = 1/10th of a disk image at a time.
// (This value MUST be < 0xff, as 0xff is used as record number to
//  invalidate the rec_b cache via setting b_idx = 0xff; )
#define QSORT_MAX_RECORDS 160
#define QSORT_STACK_SIZE  160  

typedef struct {
    unsigned char low;
    unsigned char high;
} partition_t;

struct sort_buffers {
  // Quick sort list of index numbers of the records being sorted
  unsigned char indices[QSORT_MAX_RECORDS];  // indices into your slab records
  // Quick sort manual stack
  partition_t stack[QSORT_STACK_SIZE];
  int stack_ptr;
  
  // A and B record buffers for comparison of records during quick sort
  unsigned char rec_a[RECORD_DATA_SIZE];
  unsigned char rec_b[RECORD_DATA_SIZE];
  unsigned char idx_b;
};

struct index_buffers {

  
  unsigned char rec[RECORD_DATA_SIZE];
};

#define SEARCH_MAX_QUERY_LENGTH 500
struct search_buffers {

  // Scores of all records, even unused ones
  unsigned char all_scores[USABLE_SECTORS_PER_DISK];

  // Scores and record number of all populated records
  unsigned char scores[USABLE_SECTORS_PER_DISK];
  unsigned int record_numbers[USABLE_SECTORS_PER_DISK];
  unsigned int result_count;

  unsigned int query_length;
  unsigned char query[SEARCH_MAX_QUERY_LENGTH];

  // Has score accumulation saturated for any record?
  unsigned char score_overflow;
  // If so, and we've deleted, and at least one score that
  // was 255 is now less, then we know we probably should
  // recalculate the scores
  unsigned char score_recalculation_required;

  unsigned char last_score_threshold;
  
  // Is the current search results stale?
  unsigned char results_stale;
  
  unsigned char sector_buffer[512];

  // Quick sort manual stack
  partition_t stack[QSORT_STACK_SIZE];
  int stack_ptr;

  
  // Scratch variables
  unsigned char bit;
  unsigned char byte;
  unsigned int r;
  unsigned int l;
  unsigned int index_page_offset;
  unsigned int cut_len;
};

struct telephony_buffers {
  unsigned char contact[RECORD_DATA_SIZE];

  unsigned char message[RECORD_DATA_SIZE];
  unsigned int message_bytes;
  
  unsigned char sector_buffer[512];  
};

struct textbox {  
  unsigned char record[RECORD_DATA_SIZE];

  unsigned char *field;
  unsigned int field_len;
  
  unsigned int len;
  unsigned char pixel_widths[RECORD_DATA_SIZE];
  unsigned char glyph_widths[RECORD_DATA_SIZE];
  unsigned int break_costs[RECORD_DATA_SIZE];

  unsigned int line_count;
  unsigned int line_offsets_in_bytes[RECORD_DATA_SIZE];
};

struct shared_buffers {

#define LOCK_FREE 0x00
#define LOCK_INDEX 0x01
#define LOCK_SORT 0x02
#define LOCK_SEARCH 0x03
#define LOCK_TELEPHONY 0x04
#define LOCK_TEXTBOX 0x05
  unsigned char lock;
  
  union {
    struct sort_buffers sort;
    struct index_buffers index;
    struct search_buffers search;
    struct telephony_buffers telephony;
    struct textbox textbox;
  };
};

extern struct shared_buffers buffers;

unsigned char buffers_lock(unsigned char module);
unsigned char buffers_unlock(unsigned char module);

#endif
