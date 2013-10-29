#ifndef __PGM_IO_H__
#define __PGM_IO_H__

#include <stdio.h>
#include <stdint.h>

void pgm_save(unsigned char *buf, int wrap, int xsize, int ysize,
              char *filename);

void pgm_read(char *filename, uint8_t *buf, int buf_len,
              int* xsize, int* ysize);

#endif//__PGM_IO_H__
