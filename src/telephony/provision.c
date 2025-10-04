// Load HAL
#include "includes.h"
#include "contacts.h"
#include "records.h"

/*
  Provision the entire MEGAphone directory structure and phone state.

  Currently works only on Linux, not natively on the MEGA65, due to the
  lack of working MKDIR hypervisor trap.
*/

#define MAX_CONTACTS 1600

// XXX - Make testing faster for now
#undef MAX_CONTACTS
#define MAX_CONTACTS 100  

int main(int argc,char **argv)
{
  unsigned short contact;
  char hex[2];

  if (argc>1) {
    if (chdir(argv[1])) {
      fprintf(stderr,"FATAL: Could not chdir('%s')\n",argv[1]);
      exit(-1);
    }
  }
  
  hal_init();
  mega65_mkdir("PHONE");
  mega65_chdir("PHONE");
  mega65_mkdir("THREADS");

  /* For fast traversal (i.e., short directory listings) we
     create 4 layers deep of 0-F directory names.
     That allows for 65,536 contacts. Given that we only
     support 1,560 contacts in other parts of the system for
     now, that's safe. I could have limited it to 3 layers and
     4,096 contacts, but we do have a mechanism to increase the
     contact count to ~15,600, so 4 layers is just safer.

     Then inside each of those directories we need a MESSAGES.D81
     and MSGINDEX.D81 for indexing the message D81s (providing quick
     search facility via an index of diphtongs occurring in the
     messages).

     While we'll create the 4-level directory structure, we will
     only populate the first 1,580 with those files, so that we don't
     eat up an insane amount of SD card. It will already be a few GB,
     as it is.
  */

  // D81 for the phone state
  create_d81("STATE.D81");
  mount_d81("STATE.D81",0);
  format_image_fully_allocated(0,"PHONE STATE",NO_SECTOR_MARKERS);

  // And for the first 1,560 contacts.
  create_d81("CONTACT0.D81");
  mount_d81("CONTACT0.D81",0);
  format_image_fully_allocated(0,"CONTACTS 0", WITH_SECTOR_MARKERS);
  // Create dummy "UNKNOWN" contact in record 1
  unsigned char buffer[RECORD_DATA_SIZE];
  unsigned int bytes_used=0;
  if (build_contact(buffer,&bytes_used,
		    (unsigned char *)"UNKNOWN NUMBERS",
		    (unsigned char *)"UNKNOWN NUMBERS",
		    (unsigned char *)"UNKNOWN NUMBER",0)) {
    fprintf(stderr,"FATAL: Failed to build unknown contact record.\n");
    exit(-1);
  }
  mega65_cdroot();
  mega65_chdir("PHONE");
  mount_d81("CONTACT0.D81",0);
  read_sector(0,1,0);
  unsigned int record_number = record_allocate_next( SECTOR_BUFFER_ADDRESS );
  if (record_number!=1) {
    fprintf(stderr,"FATAL: FAiled to allocate contact 1 for UNKNOWN\n");
    exit(-1);
  }
  // Write back updated BAM
  write_sector(0,1,0);
  
  // Set record number bytes in record
  buffer[0]=record_number & 0xff;
  buffer[1]=record_number >> 8;
  
  // Allocated record, so write contact	  
  char r=write_record_by_id(0,record_number,buffer);
  if (r) {
    fprintf(stderr,"ERROR: Failed to write contact to record #%d (code %d)\n",record_number,r);
  }    
  
  // Then we need the different sorted versions of the contacts,
  create_d81("SORT02-0.D81");
  create_d81("SORT04-0.D81");
  create_d81("SORT06-0.D81");
  // And the character pair lookup indexes
  create_d81("IDX02-0.D81");  // by first name
  create_d81("IDX04-0.D81");  // by last name
  create_d81("IDX06-0.D81");  // by phone number
  create_d81("IDXALL-0.D81");  // by all fields concatenated

  // And scratch disk image used during sorting
  create_d81("SCRATCH.D81");

  for(contact=0;contact<MAX_CONTACTS;contact++) {

    printf("Populating directory structure for contact #%d\n",contact);
    
    // For compatibiity with MEGA65 HAL, we can't
    // CD .. or use relative paths. So we have to
    // slightly inefficiently go all the way back to the root
    // directory and make the next directory.
    // This will be SLOW -- at least until we get hardware
    // accelerated FAT filesystem stuff and/or the SD card
    // cache, as there will be lots of directory traversals
    // going on.  Such is life.
    mega65_cdroot();
    if (mega65_chdir("PHONE")) exit(-1);
    if (mega65_chdir("THREADS")) exit(-1);
    hex[0]=to_hex(contact>>12);
    hex[1]=0;
    mega65_mkdir(hex);
    if (mega65_chdir(hex)) exit (-1);
    hex[0]=to_hex(contact>>8);
    hex[1]=0;
    mega65_mkdir(hex);
    if (mega65_chdir(hex)) exit (-1);
    hex[0]=to_hex(contact>>4);
    hex[1]=0;
    mega65_mkdir(hex);
    if (mega65_chdir(hex)) exit (-1);
    hex[0]=to_hex(contact>>0);
    hex[1]=0;
    mega65_mkdir(hex);
    if (mega65_chdir(hex)) exit (-1);

    create_d81("MESSAGES.D81");
    create_d81("MSGINDEX.D81");
    mount_d81("MESSAGES.D81",0);
    format_image_fully_allocated(0,"MESSAGES", WITH_SECTOR_MARKERS);
    mount_d81("MSGINDEX.D81",0);
    format_image_fully_allocated(0,"MESSAGE INDEX", NO_SECTOR_MARKERS);
    
  }
     
}
