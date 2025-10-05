#include "includes.h"
#include "buffers.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <time.h>

int verbose = 0;

unsigned char tof_r;

unsigned char sector_buffer[512];
unsigned char work_buffer[WORK_BUFFER_SIZE];

#define MAX_DRIVES 2
FILE *drive_files[MAX_DRIVES]={NULL};

#define PATH_LEN 2048
char working_directory[PATH_LEN];

void hal_init(void)
{
  if (!getcwd(working_directory,PATH_LEN)) {
    fprintf(stderr,"FATAL: Failed to read current working directory in hal_init()\n");
    perror("getcwd()");
    exit(-1);
  }

  // Start with shared data structure unlocked
  buffers.lock = LOCK_FREE;
}

char to_hex(unsigned char v)
{
  v&=0xf;
  if (v<0xa) return v+'0';
  if (v>0xf) return 0;
  return 'A'+(v-0xa);
}

unsigned char de_bcd(unsigned char in)
{
  return (in &0xf) + (in>>4)*10;  
}

char mega65_mkdir(char *dir)
{
  char cwd[2048];
  if (verbose)
    fprintf(stderr,"INFO: Making directory '%s' in '%s'\n",
	    dir,getcwd(cwd,sizeof(cwd)));
  return mkdir(dir,0750);
}

char mega65_cdroot(void)
{
  if (verbose)
    fprintf(stderr,"INFO: CDROOT: Changing directory to '%s'\n",
	    working_directory);
  return chdir(working_directory);
}

char mega65_chdir(char *dir)
{
  char cwd[2048];
  if (verbose)
    fprintf(stderr,"INFO: Changing directory to '%s' in '%s'\n",
	    dir,getcwd(cwd,sizeof(cwd)));
  int r= chdir(dir);
  if (r) {perror("chdir()");
    fprintf(stderr,"INFO: Requested directory was '%s'\n",dir);
  }
  return r;
}

uint16_t to_bcd(unsigned int in)
{
  return (in%10) + 0x10 * ((in/10)%10) + 0x100 * ((in/100)%10) + 0x1000 * ((in/1000)%10);
}

unsigned long mega65_bcddate(void)
{
  // Format is 32-bit packed time.

  // Naive would be:
  // YEAR  = 16 bits BCD!
  // MONTH = 8 bits BCD!
  // DAY   = 8 bits BCD!

  // Format is 32-bit BCD packed time (24 hour time)

  time_t now;
  struct tm *local;
  time(&now);
  local = localtime(&now);
 
  unsigned long bcd_year, bcd_month, bcd_day;

  bcd_year = to_bcd(local->tm_year + 1900);
  bcd_month = to_bcd(local->tm_mon + 1);
  bcd_day = to_bcd(local->tm_mday);
  
  return (((unsigned long)bcd_year)<<16) + (bcd_month << 8) + bcd_day;
}

unsigned long mega65_bcdtime(void)
{ 
  // Format is 32-bit BCD packed time (24 hour time)

  time_t now;
  struct tm *local;
  time(&now);
  local = localtime(&now);
 
  unsigned long bcd_hour, bcd_min, bcd_sec;

  bcd_hour = to_bcd(local->tm_hour);
  bcd_min = to_bcd(local->tm_min);
  bcd_sec = to_bcd(local->tm_sec);
    
  return (((unsigned long)(bcd_hour))<<16) + (bcd_min<<8) + bcd_sec;
}



void lpoke(unsigned long long addr, unsigned char val)
{
  unsigned char *a = (unsigned char *)addr;
  a[0]=val;
}

unsigned char lpeek(unsigned long long addr)
{
  unsigned char *a = (unsigned char *)addr;
  return a[0];
}


void lfill(unsigned long long addr, unsigned char val, unsigned int len)
{
  if (!len) {
    fprintf(stderr,"FATAL: lfill() length = 0, which on the MEGA65 means 64KB.\n");
    exit(-1);
  }

  memset((unsigned char *)addr,val,len);
  
}

void lcopy(unsigned long long src, unsigned long long dest, unsigned int len)
{
  if (!len) {
    fprintf(stderr,"FATAL: lcopy() length = 0, which on the MEGA65 means 64KB.\n");
    exit(-1);
  }

  memmove((unsigned char *)dest,(unsigned char *)src,len);
}

char read_sector(unsigned char drive_id, unsigned char track, unsigned char sector)
{
  unsigned int offset = (track-1)*(2*10*512) + (sector*512);

  if (track<1||track>80||sector<0||sector>19) {
    fprintf(stderr,"FATAL: Illegal track and/or sector: T%d, S%d\n",track,sector);
    exit(-1);
  }
  
  if (offset>=(800*1024)) {
    fprintf(stderr,"FATAL: Track %d, Sector %d resolved to address 0x%08x\n",track, sector, offset);
    exit(-1);
  }

  if (drive_id>1) {
    fprintf(stderr,"FATAL: Illegal drive_id=%d in read_sector()\n",drive_id);
    return 1;
  }
  if (!drive_files[drive_id]) {
    fprintf(stderr,"FATAL: read_sector() called on drive %d, but it is not mounted.\n",drive_id);
    return 2;
  }

  if (fseek(drive_files[drive_id],offset,SEEK_SET)) {
    fprintf(stderr,"FATAL: read_sector(%d,%d,%d) failed to seek to offset 0x%06x.\n",drive_id,track,sector,offset);
    perror("read_sector()");
    return 3;
  }
  
  if (fread(sector_buffer,512,1,drive_files[drive_id])!=1) {
    fprintf(stderr,"FATAL: read_sector(%d,%d,%d) at offset 0x%06x failed.\n",drive_id,track,sector,offset);
    perror("read_sector()");
    return 4;
  }

  return 0;
}

char write_sector(unsigned char drive_id, unsigned char track, unsigned char sector)
{
  unsigned int offset = (track-1)*(2*10*512) + (sector*512);

  if (track<1||track>80||sector<0||sector>19) {
    fprintf(stderr,"FATAL: Illegal track and/or sector: T%d, S%d\n",track,sector);
    exit(-1);
  }
  
  if (offset>=(800*1024)) {
    fprintf(stderr,"FATAL: Track %d, Sector %d resolved to address 0x%08x\n",track, sector, offset);
    exit(-1);
  }
  
  if (drive_id>1) {
    fprintf(stderr,"FATAL: Illegal drive_id=%d in write_sector()\n",drive_id);
    return 1;
  }
  if (!drive_files[drive_id]) {
    fprintf(stderr,"FATAL: write_sector() called on drive %d, but it is not mounted.\n",drive_id);
    return 2;
  }

  if (fseek(drive_files[drive_id],offset,SEEK_SET)) {
    fprintf(stderr,"FATAL: write_sector(%d,%d,%d) failed to seek to offset 0x%06x.\n",drive_id,track,sector,offset);
    perror("write_sector()");
    return 3;
  }
  
  if (fwrite(sector_buffer,512,1,drive_files[drive_id])!=1) {
    fprintf(stderr,"FATAL: write_sector(%d,%d,%d) at offset 0x%06x failed.\n",drive_id,track,sector,offset);
    perror("write_sector()");
    return 4;
  }

  return 0;
}

char mount_d81(char *filename, unsigned char drive_id)
{
  if (drive_id >= MAX_DRIVES) {
    fprintf(stderr,"ERROR: Attempted to mount a disk image to drive %d (must be 0 -- %d)\n",
	    drive_id, MAX_DRIVES-1);
    return -1;
  }
  if (drive_files[drive_id]) { fclose(drive_files[drive_id]); drive_files[drive_id]=NULL; }

  drive_files[drive_id]=fopen(filename,"rb+");
  if (!drive_files[drive_id]) {
    fprintf(stderr,"ERROR: Failed to mount '%s' as drive %d\n",filename,drive_id);
    perror("fopen()");
    return -1;
  }

  if (verbose)
    fprintf(stderr,"INFO: Disk image '%s' mounted as drive %d\n",filename,drive_id);
  
  return 0;
}

char create_d81(char *filename)
{
  unlink(filename);
  FILE *f=fopen(filename,"rb");
  if (f) {
    if (verbose) fprintf(stderr,"ERROR: Disk image '%s' already exists.\n",filename);
    fclose(f);
    return -1;
  }

  f=fopen(filename,"wb");
  if (!f) {
    fprintf(stderr,"ERROR: Failed to create disk image '%s'\n",filename);
    perror("fopen()");
    return -1;
  }

  if (ftruncate(fileno(f), 800*1024)) {  
    fprintf(stderr,"ERROR: Failed to set size of newly created disk image '%s'\n",filename);
    perror("ftruncate()");
    return -1;
  }


  fclose(f);
  
  return 0;
}



unsigned char as_printable(unsigned char c)
{
  if (c<' ') return '.';
  if (c>=0x7e) return '.';
  return c;
}

void dump_bytes(char *msg, unsigned long d_in, int len)
{
  unsigned char *d = (unsigned char *)d_in;
  
  fprintf(stderr,"DEBUG: %s\n",msg);
  for(int i=0;i<len;i+=16) {
    fprintf(stderr,"%04X: ",i);
    for(int j=0;j<16;j++) {
      if ((i+j)<len) {
	fprintf(stderr,"%02X ",d[i+j]);
      } else fprintf(stderr,"   ");
    }
    fprintf(stderr,"  ");
    for(int j=0;j<16;j++) {
      if ((i+j)<len) {
	fprintf(stderr,"%c",as_printable(d[i+j]));
      } 
    }
    fprintf(stderr,"\n");
  }
}

void dump_sector_buffer(char *m) {
  dump_bytes(m,sector_buffer, 512);
}


char log_error_(const char *file,const char *func,const unsigned int line,const unsigned char error_code)
{
  fprintf(stderr,"%s:%d:%s(): Returning with error %d\n",
	  file,line,func,error_code);
  return error_code;
}
