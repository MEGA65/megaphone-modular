
#ifndef INCLUDES_H
#define INCLUDES_H

#include "mega65/hal.h"
#include "mega65/shres.h"
#include "mega65/memory.h"

#define SECTOR_BUFFER_ADDRESS 0xFFD6E00L

#define WORK_BUFFER_SIZE (128*1024)
#define WORK_BUFFER_ADDRESS 0x12000L

void hal_init(void);
char write_sector(unsigned char drive_id, unsigned char track, unsigned char sector);
char read_sector(unsigned char drive_id, unsigned char track, unsigned char sector);
char mount_d81(char *filename, unsigned char drive_id);
char create_d81(char *filename);
char mega65_mkdir(char *dir);
char mega65_cdroot(void);
char mega65_chdir(char *dir);

#define WITH_SECTOR_MARKERS 1
#define NO_SECTOR_MARKERS 0
void format_image_fully_allocated(char drive_id,char *header, char withSectorMarkers);

char sort_d81(char *name_in, char *name_out, unsigned char field_id);

void dump_sector_buffer(char *m);
void dump_bytes(char *msg, unsigned char *d, int len);

char log_error_(const char *file,const char *func,const unsigned int line,const unsigned char error_code);
#define fail(X)

#endif

