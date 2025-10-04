#ifndef INCLUDES_H
#define INCLUDES_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#define CROSS_COMPILED

extern unsigned char sector_buffer[512];
#define SECTOR_BUFFER_ADDRESS ((unsigned long long) &sector_buffer[0])

#define WORK_BUFFER_SIZE (128*1024)
extern unsigned char work_buffer[WORK_BUFFER_SIZE];
#define WORK_BUFFER_ADDRESS ((unsigned long long) &work_buffer[0])

void hal_init(void);
void lpoke(unsigned long long addr, unsigned char val);
unsigned char lpeek(unsigned long long addr);
void lfill(unsigned long long addr, unsigned char val, unsigned int len);
void lcopy(unsigned long long src, unsigned long long dest, unsigned int len);
char write_sector(unsigned char drive_id, unsigned char track, unsigned char sector);
char read_sector(unsigned char drive_id, unsigned char track, unsigned char sector);
char mount_d81(char *filename, unsigned char drive_id);
char create_d81(char *filename);
char mega65_mkdir(char *dir);
char mega65_cdroot(void);
char mega65_chdir(char *dir);

unsigned long mega65_bcddate(void);
unsigned long mega65_bcdtime(void);

#define WITH_SECTOR_MARKERS 1
#define NO_SECTOR_MARKERS 0
void format_image_fully_allocated(char drive_id,char *header, char withSectorMarkers);

char sort_d81(char *name_in, char *name_out, unsigned char field_id);

void dump_sector_buffer(char *m);
void dump_bytes(char *msg, unsigned char *d, int len);

char to_hex(unsigned char v);

char log_error_(const char *file,const char *func,const unsigned int line,const unsigned char error_code);
#define fail(X) return(log_error_(__FILE__,__FUNCTION__,__LINE__,X))

extern unsigned char tof_r;
#define try_or_fail(X) if ((tof_r=X)!=0) fail(tof_r)

#endif

