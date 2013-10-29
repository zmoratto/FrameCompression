#include <rejigger.h>

#define SMALL_WIDTH 640
#define SMALL_HEIGHT 896

#define LARGE_WIDTH 1280
#define LARGE_HEIGHT 1168

void rejigger_small_frame(AVFrame *in_frame, AVFrame *out_frame) {
  int i, j;

  // Copy the wide angle camera
  for ( i = 0; i < 480; i++ ) {
    memcpy(out_frame->data[0]+out_frame->linesize[0]*i,
           in_frame->data[0]+in_frame->linesize[0]*(i+32),
           SMALL_WIDTH);
  }

  // Copy the depth camera's odd bytes to location (0,480)
  for ( j = 0; j < 180; j++ ) {
    for ( i = 0; i < 320; i++ ) {
      *(out_frame->data[0] + (j+480)*out_frame->linesize[0] + i) =
        *(in_frame->data[0] + (j+704)*in_frame->linesize[0] + 2*i);
    }
  }

  // Copy the depth camera's even bytes to location (320,480)
  for ( j = 0; j < 180; j++ ) {
    for ( i = 0; i < 320; i++ ) {
      *(out_frame->data[0] + (j+480)*out_frame->linesize[0] + i + 320) =
        *(in_frame->data[0] + (j+704)*in_frame->linesize[0] + 2*i + 1);
    }
  }
}

void unjigger_small_frame(AVFrame *in_frame, AVFrame *out_frame) {
}
