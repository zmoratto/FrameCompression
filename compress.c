#include <stdio.h>
#include <string.h>

#include <libavcodec/avcodec.h>
#include <libavutil/mathematics.h>
#include <libavformat/avformat.h>

static void pgm_save(unsigned char *buf, int wrap, int xsize, int ysize,
                     char *filename) {
    FILE *f;
    int i;

    f=fopen(filename,"w");
    fprintf(f,"P5\n%d %d\n%d\n",xsize,ysize,255);
    for(i=0;i<ysize;i++)
        fwrite(buf + i * wrap,1,xsize,f);
    fclose(f);
}

static void pgm_read(char *filename, uint8_t *buf, int buf_len, int* xsize, int* ysize ) {
  // Reads a PGM file into buffer above
  FILE *f;
  int i;

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
  fscanf(f, "%i %i\n", xsize, ysize);

  printf("Found image of size %ix%i\n", *xsize, *ysize);

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

static int open_codec_context(int *stream_idx,
                              AVFormatContext *fmt_ctx, enum AVMediaType type)
{
    int ret;
    AVStream *st;
    AVCodecContext *dec_ctx = NULL;
    AVCodec *dec = NULL;

    ret = av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not find %s stream in input file '%s'\n",
                av_get_media_type_string(type), "data/m%07d.pgm");
        return ret;
    } else {
        *stream_idx = ret;
        st = fmt_ctx->streams[*stream_idx];

        /* find decoder for the stream */
        dec_ctx = st->codec;
        dec = avcodec_find_decoder(dec_ctx->codec_id);
        if (!dec) {
            fprintf(stderr, "Failed to find %s codec\n",
                    av_get_media_type_string(type));
            return ret;
        }

        if ((ret = avcodec_open2(dec_ctx, dec, NULL)) < 0) {
            fprintf(stderr, "Failed to open %s codec\n",
                    av_get_media_type_string(type));
            return ret;
        }
    }

    return 0;
}

int main( int argc, char ** argv ) {
  size_t pgm_array_length = 1280 * 1168;
  uint8_t* pgm_array = (uint8_t*) malloc( pgm_array_length );
  int xsize, ysize, i, ret, got_output;

  AVCodec *codec;
  AVCodecContext *c = NULL;
  FILE *f;
  AVFrame *frame;
  AVPacket pkt;
  uint8_t endcode[] = { 0, 0, 1, 0xb7 };

  av_register_all();

  // testing
  AVFormatContext *fmt_ctx = NULL;
  if (avformat_open_input(&fmt_ctx, "data/m%07d.pgm", NULL, NULL) < 0) {
    fprintf(stderr, "Couldn't open input source");
    exit(1);
  }

  if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
    fprintf(stderr, "Could not find stream information\n");
    exit(1);
  }

  int video_stream_idx = -1;
  open_codec_context(&video_stream_idx, fmt_ctx, AVMEDIA_TYPE_VIDEO);
  AVStream *video_stream = fmt_ctx->streams[video_stream_idx];
  AVCodecContext *video_dec_ctx = video_stream->codec;

  printf("Input size %i %i\n", video_dec_ctx->width, video_dec_ctx->height );

  exit(1);

  // Find our codec
  codec = avcodec_find_encoder( AV_CODEC_ID_H264 );
  if ( !codec ) {
    fprintf(stderr, "Codec not found\n");
    exit(1);
  }

  c = avcodec_alloc_context3( codec );
  if ( !c ) {
    fprintf(stderr, "Could not allocate video codec context\n" );
    exit(1);
  }

  c->bit_rate = 400000;
  // Resolution must be a multiple of 2
  c->width = 640;
  c->height = 896;
  // Frames per second
  c->time_base = (AVRational){1,25};
  c->gop_size = 10; // Emit one intra frame every ten frames?
  c->max_b_frames = 1;
  c->pix_fmt = AV_PIX_FMT_YUV420P;
  // Tell 'em to do a good job compressing
  av_opt_set(c->priv_data, "preset", "slow", 0 );

  /* open it */
  if (avcodec_open2(c, codec, NULL) < 0) {
    fprintf(stderr, "Could not open codec\n");
    exit(1);
  }

  f = fopen("test.mkv", "wb");
  if ( !f ) {
    fprintf(stderr, "Could not open output file\n");
    exit(1);
  }

  frame = avcodec_alloc_frame();
  if (!frame) {
    fprintf(stderr, "Could not allocate video frame\n");
    exit(1);
  }
  frame->format = c->pix_fmt;
  frame->width  = c->width;
  frame->height = c->height;

  /* the image can be allocated by any means and av_image_alloc() is
   * just the most convenient way if av_malloc() is to be used */
  ret = av_image_alloc(frame->data, frame->linesize, c->width, c->height,
                       c->pix_fmt, 32);
  if (ret < 0) {
    fprintf(stderr, "Could not allocate raw picture buffer\n");
    exit(1);
  }

  printf("Line size is: %ix%ix%i\n", frame->linesize[0], frame->linesize[1], frame->linesize[2] );

  // Encode 1 second of video
  for ( i = 0; i < 25; i++ ) {
    pgm_read( "data/m0000639.pgm", pgm_array, pgm_array_length, &xsize, &ysize );

    av_init_packet(&pkt);
    pkt.data = NULL;   // packet data will be allocated by the encoder
    pkt.size = 0;
    fflush(stdout);

    // Write frame
    memcpy( frame->data[0], pgm_array, xsize * ysize );
    frame->pts = i;

    // Encode the image
    ret = avcodec_encode_video2(c, &pkt, frame, &got_output);
    if (ret < 0) {
      fprintf(stderr, "Error encoding frame\n");
      exit(1);
    }

    if (got_output) {
      printf("Write frame %3d (size=%5d)\n", i, pkt.size);
      fwrite(pkt.data, 1, pkt.size, f);
      av_free_packet(&pkt);
    }

  }

  /* get the delayed frames */
  for (got_output = 1; got_output; i++) {
    fflush(stdout);

    ret = avcodec_encode_video2(c, &pkt, NULL, &got_output);
    if (ret < 0) {
      fprintf(stderr, "Error encoding frame\n");
      exit(1);
    }

    if (got_output) {
      printf("Write frame %3d (size=%5d)\n", i, pkt.size);
      fwrite(pkt.data, 1, pkt.size, f);
      av_free_packet(&pkt);
    }
  }

  fwrite(endcode, 1, sizeof(endcode), f);
  fclose(f);

  avcodec_close(c);
  av_free(c);
  av_freep(&frame->data[0]);
  avcodec_free_frame(&frame);
  printf("\n");

  free(pgm_array);

  return 0;
}
