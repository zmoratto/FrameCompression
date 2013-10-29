#include <pgm_io.h>

void skip_comment( FILE* f ) {
  char c = fgetc( f );
  if ( c == '#' ) {
    // Eat entire line
    while ( fgetc( f ) != '\n' ) {}
  } else {
    fseek(f,-1,SEEK_CUR);
  }
}

void pgm_save(uint8_t *buf, int wrap, int xsize, int ysize,
              char *filename) {
  FILE *f;
  int i;

  f=fopen(filename,"w");
  fprintf(f,"P5\n%d %d\n%d\n",xsize,ysize,255);
  for(i=0;i<ysize;i++)
    fwrite(buf + i * wrap,1,xsize,f);
  fclose(f);
}

void pgm_read(char *filename, uint8_t *buf, int buf_len, int* xsize, int* ysize
              ) {
  // Reads a PGM file into buffer above
  FILE *f;
  int i, depth;

  f=fopen(filename,"rb");
  fread( buf, 1, 3, f );

  // Verify we have a PGM file for an input
  if ( buf[0] != 'P' || buf[1] != '5' || buf[2] != '\n' ) {
    fprintf(stderr, "Fatal Error: Not a PGM file\n" );
    *xsize = 0;
    *ysize = 0;
    return;
  }

  // Determine size
  skip_comment(f);
  fscanf(f, "%i %i\n", xsize, ysize);
  //printf("Found image of size %ix%i\n", *xsize, *ysize);
  skip_comment(f);
  fscanf(f, "%i\n", &depth );
  //printf("Depth is %i\n", depth );

  // Verify this fits in buffer
  if ( (*xsize) * (*ysize) > buf_len ) {
    fprintf(stderr, "Fatal Error: Can\'t fit input image inside of buffer\n" );
    *xsize = 0;
    *ysize = 0;
    return;
  }

  // Reading
  for ( i = 0; i < *ysize; i++ ) {
    fread( buf + i * (*xsize), 1, *xsize, f );
  }

  // Finish up
  fclose(f);
}
