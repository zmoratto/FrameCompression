#ifndef __DEPTH_CODE_H__
#define __DEPTH_CODE_H__

#include <stdint.h>

// This is modified version based on:
// Adapting Standard Video Codec for Depth Streaming
// F. Pece, J. Kautz, T. Weyrich
void depth_to_yuv(uint16_t depth, uint8_t *y, uint8_t *u );

void yuv_to_depth(uint8_t y, uint8_t u, uint16_t *depth);

#endif//__DEPTH_CODE_H__
