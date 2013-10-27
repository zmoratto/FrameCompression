#include <stdio.h>
#include <string.h>

#include <libavcodec/avcodec.h>
#include <libavutil/mathematics.h>
#include <libavutil/imgutils.h>
#include <libavutil/timestamp.h>
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

int main( int argc, char ** argv ) {
  int xsize, ysize, i, got_frame;

  // Used by AVFormat to load up our input stream
  AVFormatContext *fmt_ctx = NULL;

  // Video storage
  int video_stream_idx = -1;
  int video_frame_count = 0;
  static uint8_t *video_dst_data[4] = {NULL};
  static int      video_dst_linesize[4];
  static int      video_dst_bufsize;
  AVCodec *video_codec;
  AVCodecContext *video_codec_ctx = NULL;

  // Used by all
  AVFrame *frame = NULL;
  AVPacket pkt;

  static const char input_name[] = "data/m%07d.pgm";

  // Register all formats and codecs
  av_register_all();

  // Open input file ... which is actually a stream of PGM files. This
  // should later be something that the user can provide.
  if (avformat_open_input(&fmt_ctx, input_name, NULL, NULL) < 0) {
    fprintf(stderr, "Couldn't open input source");
    exit(1);
  }

  // Retrieve stream information
  if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
    fprintf(stderr, "Could not find stream information\n");
    exit(1);
  }

  // Open Codec Stream
  if ((video_stream_idx = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0)) < 0) {
    fprintf(stderr, "Could not find video stream in input file.");
    exit(1);
  }

  // Find the decoder for this stream and open it
  AVStream *stream = fmt_ctx->streams[video_stream_idx];
  video_codec_ctx = stream->codec;
  video_codec = avcodec_find_decoder(video_codec_ctx->codec_id);
  if ( !video_codec ) {
    fprintf(stderr, "Failed to find video codec\n");
    exit(1);
  }

  if (avcodec_open2(video_codec_ctx, video_codec, NULL) < 0) {
    fprintf(stderr, "Failed to open video codec\n");
    exit(1);
  }

  // Allocate space for our video frames we are reading
  video_dst_bufsize =
    av_image_alloc(video_dst_data, video_dst_linesize,
                   video_codec_ctx->width, video_codec_ctx->height,
                   video_codec_ctx->pix_fmt, 1 );

  // Allocate space for frame
  frame = avcodec_alloc_frame();
  if (!frame) {
    fprintf(stderr, "Could not allocate frame\n");
    exit(1);
  }

  // Initialize packet, set data to NULL so demuxer fills it in
  av_init_packet(&pkt);
  pkt.data = NULL;
  pkt.size = 0;

  // Read frames from the file
  while (av_read_frame(fmt_ctx, &pkt) >= 0) {

    // Decode video
    if (pkt.stream_index == video_stream_idx ) {
      if ( avcodec_decode_video2(video_codec_ctx, frame, &got_frame, &pkt) <= 0 ) {
        fprintf(stderr, "Error decoding video frame\n");
        exit(1);
      }

      if (got_frame) {
        printf("video_frame n:%d coded_n:%d pts:%s\n",
               video_frame_count++, frame->coded_picture_number,
               av_ts2timestr(frame->pts, &video_codec_ctx->time_base));

        // I should decode the buffer and write a pgm here
      }
    }

    av_free_packet(&pkt);
  }


  // Kill everything
  if (video_codec_ctx)
    avcodec_close(video_codec_ctx);
  avformat_close_input(&fmt_ctx);
  av_free(frame);
  av_free(video_dst_data[0]);

  return 0;
}
