#include <rejigger.h>
#include <depth_code.h>

#define SMALL_WIDTH 640
#define SMALL_HEIGHT 896

#define LARGE_WIDTH 1280
#define LARGE_HEIGHT 1168

void rejigger_small_frame(uint8_t *in_data, int in_linesize,
                          uint8_t *out_data, int out_linesize ) {
  int i, j;

  // Copy the wide angle camera
  for ( i = 0; i < 480; i++ ) {
    memcpy(out_data+out_linesize*i,
           in_data+in_linesize*(i+32),
           SMALL_WIDTH);
  }

  // Move the depth camera
  for ( j = 0; j < 180; j++ ) {
    for ( i = 0; i < 320; i++ ) {
      depth_to_yuv(*(uint16_t*)(in_data + (j + 704) * in_linesize + 2 * i ),
                   out_data + (j + 480) * out_linesize + i,
                   out_data + (j + 480) * out_linesize + i + 320 );
    }
  }
}

void unjigger_small_frame(uint8_t *in_data, int in_linesize,
                          uint8_t *out_data, int out_linesize ) {
  int i, j;

  // Move the wide angle into the correct spot
  for ( i = 0; i < 480; i++ ) {
    memcpy( out_data+out_linesize*(32+i),
            in_data+in_linesize*i,
            SMALL_WIDTH );
  }

  // Move the depth camera's even bytes back
  for ( j = 0; j < 180; j++ ) {
    for ( i = 0; i < 320; i++ ) {
      yuv_to_depth(*(in_data + (j + 480) * in_linesize + i),
                   *(in_data + (j + 480) * in_linesize + i + 320),
                   (uint16_t*)(out_data + (j + 704) * out_linesize + 2 * i ) );
    }
  }
}
