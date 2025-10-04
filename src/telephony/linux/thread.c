#include "includes.h"
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "buffers.h"
#include "search.h"
#include "contacts.h"

void usage(void)
{
  fprintf(stderr,"usage: thread <contact name or number> [ranked] [message thread filter]\n");
  exit(-1);
}

int main(int argc,char **argv)
{
  if (argc<2) usage();

  hal_init();

  char *query=NULL;
  int ranked=0;
  
  if (argc==4) {
    if (!strcasecmp("ranked",argv[2])) ranked=1;
    else {
      usage();
    }
    query=argv[3];
  }
  if (argc==3) {
    if (!strcasecmp("ranked",argv[2])) ranked=1;
    else query=argv[2];      
  }

  mega65_chdir("PHONE");
    
  // Mount disk and index
  if (mount_d81("CONTACT0.D81",0)) return 1;
  if (mount_d81("IDXALL-0.D81",1)) return 1;

  unsigned int score_threshold=1;

  // Dodgy match selecitivity filter  
  if (argv[1]&&(strlen(argv[1]?argv[1]:"")>3))
    score_threshold=strlen(argv[1])*0.75;

  // Get list of candidate contacts
  if (search_query_init()) return 3;
  if (argc>1&&argv[1]) search_query_append_string((unsigned char *)argv[1]);
  search_collate(score_threshold); // apply match threshold filter
  search_sort_results_by_score();

  if (!buffers.search.result_count) {
    fprintf(stderr,"ERROR: No matching contacts found.\n");
    exit(-1);
  }

  for(int i=0;i<buffers.search.result_count;i++) {
    unsigned char record[RECORD_DATA_SIZE];
    if (read_record_by_id(0,buffers.search.record_numbers[i], record))
      printf("<could not read record>\n");
    else {
      unsigned int firstNameLen, lastNameLen, phoneNumberLen, unreadCountLen;
      unsigned char *firstName = find_field(record,RECORD_DATA_SIZE,FIELD_FIRSTNAME,&firstNameLen);
      unsigned char *lastName = find_field(record,RECORD_DATA_SIZE,FIELD_LASTNAME,&lastNameLen);
      unsigned char *phoneNumber = find_field(record,RECORD_DATA_SIZE,FIELD_PHONENUMBER,&phoneNumberLen);
      unsigned char *unreadCount = find_field(record,RECORD_DATA_SIZE,FIELD_UNREAD_MESSAGES,&unreadCountLen);
      int unreadMessageCount=0;
      if (unreadCount) unreadMessageCount=unreadCount[0]+(unreadCount[1]<<8);
      if (firstName||lastName||phoneNumber) {
	printf("CONTACT:%s:%s:%s:%d\n",
	       firstName?(char *)firstName:"<no first name>",
	       lastName?(char *)lastName:"<no last name>",
	       phoneNumber?(char *)phoneNumber:"<no phone number>",
	       unreadMessageCount);

	// XXX -- The following stomps the search state.
	// We have to either save it somewhere and restore it after,
	// or return results for only a single record (= contact)
	
	// Now reset the query and open the conversation
	mount_contact_qso(buffers.search.record_numbers[i]);

	search_query_init();
	score_threshold = 1;
	if (query) {
	  if (query&&(strlen(query)>3))
	    score_threshold=strlen(query)*0.75;
	}
	if (query) search_query_append_string((unsigned char *)query);

	search_collate(score_threshold);
	if (ranked) search_sort_results_by_score();

	for(int i=0;i<buffers.search.result_count;i++) {
	  unsigned char record[RECORD_DATA_SIZE];
	  if (read_record_by_id(0,buffers.search.record_numbers[i], record))
	    printf("<could not read record>\n");
	  else {
	    unsigned int phoneNumberLen, bodyTextLen, messageDirectionLen, timestampLen;
	    unsigned char *bodyText = find_field(record,RECORD_DATA_SIZE,FIELD_BODYTEXT,&bodyTextLen);
	    unsigned char *phoneNumber = find_field(record,RECORD_DATA_SIZE,FIELD_PHONENUMBER,&phoneNumberLen);
	    unsigned char *messageDirection = find_field(record,RECORD_DATA_SIZE,FIELD_MESSAGE_DIRECTION,&messageDirectionLen);
	    unsigned char *bcdTime = find_field(record,RECORD_DATA_SIZE,FIELD_BCDTIME,&timestampLen);
	    unsigned char *bcdDate = find_field(record,RECORD_DATA_SIZE,FIELD_BCDDATE,&timestampLen);

	    unsigned long timestampAztecTimeSec =0;

	    struct tm t;
	    t.tm_year = 2012 - 1900;
	    t.tm_mon = 12 - 1;
	    t.tm_mday = 1;
	    t.tm_hour = 0;
	    t.tm_min = 0;
	    t.tm_sec = 0;
	    if (bcdDate) {
	      t.tm_year = de_bcd(bcdDate[2]) + 100* de_bcd(bcdDate[3]) - 1900;
	      t.tm_mon  = de_bcd(bcdDate[1]) - 1;
	      t.tm_mday = de_bcd(bcdDate[0]);
	    }
	    if (bcdTime) {
	      t.tm_hour = de_bcd(bcdTime[2]);
	      t.tm_min  = de_bcd(bcdTime[1]);
	      t.tm_sec  = de_bcd(bcdTime[0]);
	    }

	    timestampAztecTimeSec = mktime(&t);
	    timestampAztecTimeSec -= 1356048000L;
	    	    
	    if (phoneNumber||bodyText||bcdTime||bcdDate) {
	      if (messageDirection[0]==SMS_DIRECTION_TX)
		printf("MESSAGETX:");
	      else
		printf("MESSAGERX:");

	      printf("%s:%ld:%s:\n",
		     phoneNumber, timestampAztecTimeSec, bodyText);
	    } else printf("<no contact fields in record>\n");
	    
	  }
	}

	fprintf(stderr,"INFO: Returning conversation only for best matching contact.\n");
	exit(0);
      }
    }
  }
  
  search_query_release();  
  
  fprintf(stderr,"INFO: Completed.\n");
  return 0;
}
