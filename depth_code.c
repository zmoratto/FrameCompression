#include <depth_code.h>
#include <stdio.h>
#include <math.h>

#define NP 512
#define HNP 256
#define QNP 128
#define W 65536

void depth_to_yuv(uint16_t depth, uint8_t *y, uint8_t *u) {
  uint16_t u0;
  *y = (depth >> 8 );
  u0 = depth % NP;
  if ( u0 >= HNP ) {
    *u = NP - u0 - 1;
  } else {
    *u = u0;
  }
}

void yuv_to_depth(uint8_t y, uint8_t u, uint16_t *depth) {
  *depth = y;
  *depth <<= 8;
  if ( y % 2 ) {
    *depth += (255 - u);
  } else {
    *depth += u;
  }
}
