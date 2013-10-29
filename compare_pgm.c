#include <pgm_io.h>
#include <stdlib.h>

int main( int argc, char **argv ) {

  int xsize1, xsize2, ysize1, ysize2;
  int buf_len1, buf_len2, buf_len3;
  uint8_t *buf1, *buf2, *buf3;
  int y, x, diff;
  uint64_t dn_diff_count = 0;
  uint8_t *ptr;

  if ( argc != 3 ) {
    fprintf(stderr, "Input required is:\n\t%s <pgm1> <pgm2>\n",
            argv[0] );
    return 1;
  }

  // Allocate enough space for 3 1280x1168 images
  buf_len3 = buf_len1 = buf_len2 = 1280 * 1168;
  buf1 = (uint8_t*) malloc( buf_len1 );
  buf2 = (uint8_t*) malloc( buf_len2 );
  buf3 = (uint8_t*) malloc( buf_len3 );

  // Load up the two pgms
  pgm_read( argv[1], buf1, buf_len1, &xsize1, &ysize1 );
  pgm_read( argv[2], buf2, buf_len2, &xsize2, &ysize2 );
  if ( xsize1 != xsize2 || ysize1 != ysize2 ) {
    fprintf(stderr, "Input images do not have the same dimensions\n");
    return 1;
  }

  // Generate comparison image and perform comparison
  for ( y = 0; y < ysize1; y++ ) {
    for ( x = 0; x < xsize1; x++ ) {
      diff = abs((int)(buf1[y*xsize1+x]) - (int)(buf2[y*xsize1+x]));
      //      printf("%i %i : %i %i\n", x, y, buf1[y*xsize1+x], buf2[y*xsize1+x]);
      dn_diff_count += diff;
      ptr = buf3 + y*xsize1 + x;
      if ( diff == 0 ) {
        *ptr = 0;
      } else if ( diff < 3 ) {
        *ptr = 128;
      } else {
        *ptr = 255;
      }
    }
  }

  // Write comparison image
  pgm_save(buf3, xsize1, xsize1, ysize1,
           "comparison.pgm" );

  printf("%lu\t%f%%\n", (long unsigned int)dn_diff_count, (double)(dn_diff_count)/((double)(xsize1*ysize1*256)/100.0));

  // Cleanup
  free( buf1 );
  free( buf2 );
  free( buf3 );

  return 0;
}
