#ifndef __REJIGGER_H__
#define __REJIGGER_H__

#include <stdint.h>

void rejigger_small_frame(uint8_t *in_data, int in_linesize,
                          uint8_t *out_data, int out_linesize );

void unjigger_small_frame(uint8_t *in_data, int in_linesize,
                          uint8_t *out_data, int out_linesize );

#endif//__REJIGGER_H__
