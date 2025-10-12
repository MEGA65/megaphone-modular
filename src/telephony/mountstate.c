#include "includes.h"

#include "records.h"
#include "mountstate.h"

uint8_t mount_state_mode;
uint16_t mount_state_contact;

char mount_state_invalidate(void)
{
  mount_state_mode = MS_NONE;
  return 0;
}

char mount_state_set(uint8_t mode, uint16_t contact)
{
  // We already have that mounted
  if (mode==mount_state_mode
      && contact == mount_state_contact) return 0;
  
  mount_state_mode = MS_NONE;     
  
  char hex[2];
  
  switch(mode) {
  case MS_CONTACT_LIST:
    
    if (contact >= USABLE_SECTORS_PER_DISK - 1 ) {
      fail(3);
      return 3;
    }
    
    try_or_fail(mega65_cdroot());
    try_or_fail(mega65_chdir("PHONE"));
    
    try_or_fail(mount_d81("CONTACT0.D81",DRIVE_0));
    
    break;
    
  case MS_CONTACT_QSO:
    
    try_or_fail(mega65_cdroot());
    
    try_or_fail(mega65_chdir("PHONE"));
    try_or_fail(mega65_chdir("THREADS"));
    
    hex[0]=to_hex(contact>>12);
    hex[1]=0;
    try_or_fail(mega65_chdir(hex));
    hex[0]=to_hex(contact>>8);
    hex[1]=0;
    try_or_fail(mega65_chdir(hex));
    hex[0]=to_hex(contact>>4);
    hex[1]=0;
    try_or_fail(mega65_chdir(hex));
    hex[0]=to_hex(contact>>0);
    hex[1]=0;
    try_or_fail(mega65_chdir(hex));
    
    try_or_fail(mount_d81("MESSAGES.D81",0));
    try_or_fail(mount_d81("MSGINDEX.D81",1));
    break;
  default:
    fail(2);
    return 2;
  }
  
  mount_state_mode = mode;
  mount_state_contact = contact;  
  
  return 0;
}
