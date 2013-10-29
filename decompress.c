#include <libavformat/avformat.h>

#include <rejigger.h>
#include <pgm_io.h>

#define BUFFER_SIZE 30

static int output_counter = 0;

static int process_full_frame( uint8_t **video_storage, int *video_read_idx, int *video_write_idx,
                               uint8_t **audio_storage, int *audio_read_idx, int *audio_write_idx ) {
  char buffer[50];

  if ( *video_read_idx == *video_write_idx ||
       *audio_read_idx == *audio_write_idx ) {
    // There is nothing to read yet
    return 1;
  }

  // Here is were we would rejigger things back to the original format. TODO

  // Move the image data down 32 rows
  memmove(video_storage[*video_read_idx]+32*640,
          video_storage[*video_read_idx], (180+480)*640);

  // Paste in the audio data
  memcpy(video_storage[*video_read_idx],
         audio_storage[*audio_read_idx], 32 * 640);

  // Write the PGM!
  sprintf(buffer,"output%07d.pgm",output_counter);
  pgm_save( video_storage[*video_read_idx], 640, 640, 896,
            buffer );

  // Increment pointers
  output_counter++;
  (*video_read_idx) = ((*video_read_idx) + 1) % BUFFER_SIZE;
  (*audio_read_idx) = ((*audio_read_idx) + 1) % BUFFER_SIZE;

  return 0;
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
    fprintf(stderr, "Could not find %s stream in input file.\n",
            av_get_media_type_string(type));
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
  int ret = 0, got_frame, i = 0;
  AVFormatContext *fmt_ctx = NULL;
  AVCodecContext *video_dec_ctx = NULL, *audio_dec_ctx = NULL;
  AVStream *video_stream = NULL, *audio_stream = NULL;

  // Stats information
  int video_stream_idx = -1;
  int audio_stream_idx = -1;
  AVFrame *frame = NULL;
  AVPacket pkt;
  int video_frame_count = 0;
  int audio_frame_count = 0;

  // Storage for decoded data. Buffered
  int video_read_idx = 0, video_write_idx = 0;
  int audio_read_idx = 0, audio_write_idx = 0;
  uint8_t *video_storage[BUFFER_SIZE]; // Assumed we only want luma
  uint8_t *audio_storage[BUFFER_SIZE]; // Assumed mono

  // Check the input
  if ( argc != 2 ) {
    fprintf(stderr,"Missing input argument\n\t%s <input.mkv>\n",
            argv[0]);
    exit(1);
  }

  // Register al formats and codecs
  av_register_all();

  /* open input file, and allocate format context */
  printf("Opening file '%s'\n",argv[1]);
  if (avformat_open_input(&fmt_ctx, argv[1], NULL, NULL) < 0) {
    fprintf(stderr, "Could not open source file %s\n", argv[1]);
    exit(1);
  }

  /* retrieve stream information */
  if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
    fprintf(stderr, "Could not find stream information\n");
    exit(1);
  }

  if (open_codec_context(&video_stream_idx, fmt_ctx, AVMEDIA_TYPE_VIDEO) >= 0) {
    video_stream = fmt_ctx->streams[video_stream_idx];
    video_dec_ctx = video_stream->codec;

    // Allocate space for storage of video data
    for ( i = 0; i < BUFFER_SIZE; i++ ) {
      video_storage[i] = malloc( video_dec_ctx->width * video_dec_ctx->height );
    }
  }

  if (open_codec_context(&audio_stream_idx, fmt_ctx, AVMEDIA_TYPE_AUDIO) >= 0) {
    audio_stream = fmt_ctx->streams[audio_stream_idx];
    audio_dec_ctx = audio_stream->codec;

    // Allocate space for storage of audio data
    for ( i = 0; i < BUFFER_SIZE; i++ ) {
      audio_storage[i] = malloc( 32 * 640 ); // This should always be 20k for us
    }
  }

  /* dump input information to stderr */
  av_dump_format(fmt_ctx, 0, argv[1], 0);

  if (!audio_stream && !video_stream) {
    fprintf(stderr, "Could not find audio or video stream in the input, aborting\n");
    ret = 1;
    return 1;
  }

  frame = avcodec_alloc_frame();
  if (!frame) {
    fprintf(stderr, "Could not allocate frame\n");
    ret = AVERROR(ENOMEM);
    return 1;
  }

  /* initialize packet, set data to NULL, let the demuxer fill it */
  av_init_packet(&pkt);
  pkt.data = NULL;
  pkt.size = 0;

  // Read frames from the file
  while ( av_read_frame(fmt_ctx, &pkt) >= 0 ) {
    if ( pkt.stream_index == video_stream_idx ) {
      video_frame_count++;

      // Decode Video Frame
      ret = avcodec_decode_video2(video_dec_ctx, frame, &got_frame, &pkt );
      if ( ret < 0 ) {
        fprintf(stderr, "Error decoding video frame\n");
        return ret;
      }

      if (got_frame) {
        for ( i = 0; i < video_dec_ctx->height; i++ ) {
          memcpy( video_storage[video_write_idx] + i*video_dec_ctx->width,
                  frame->data[0] + i*frame->linesize[0],
                  video_dec_ctx->width );
        }
        video_write_idx++;
        video_write_idx %= BUFFER_SIZE;
      }

    } else if ( pkt.stream_index == audio_stream_idx ) {
      audio_frame_count++;

      // Decode Audio
      ret = avcodec_decode_audio4(audio_dec_ctx, frame, &got_frame, &pkt );
      if ( ret < 0 ) {
        fprintf(stderr, ":Error decoding audio frame\n");
        return ret;
      }

      if ( got_frame ) {
        // The assumption is that the samples are u8 and there was no
        // compression. It was raw PCM.
        memcpy( audio_storage[audio_write_idx],
                frame->data[0],
                frame->nb_samples );
        audio_write_idx++;
        audio_write_idx %= BUFFER_SIZE;
      }
    }

    // see if we have data enough to write a pgm frame
    process_full_frame( video_storage, &video_read_idx, &video_write_idx,
                        audio_storage, &audio_read_idx, &audio_write_idx );

    printf("VT:%d R:%d W:%d | AT:%d R:%d W:%d \r",
           video_frame_count, video_read_idx, video_write_idx,
           audio_frame_count, audio_read_idx, audio_write_idx );
    fflush(stdout);

    av_free_packet(&pkt);
  }

  // Pull out the cached video frames
  pkt.data = NULL;
  pkt.size = 0;
  do {
    ret = avcodec_decode_video2(video_dec_ctx, frame, &got_frame, &pkt );
    if ( ret < 0 ) {
      fprintf(stderr, "Error decoding video frame\n");
      return ret;
    }

    if (got_frame) {
      for ( i = 0; i < video_dec_ctx->height; i++ ) {
        memcpy( video_storage[video_write_idx] + i*video_dec_ctx->width,
                frame->data[0] + i*frame->linesize[0],
                video_dec_ctx->width );
      }
      video_write_idx++;
      video_write_idx %= BUFFER_SIZE;
    }

    // see if we have data enough to write a pgm frame
    process_full_frame( video_storage, &video_read_idx, &video_write_idx,
                        audio_storage, &audio_read_idx, &audio_write_idx );

    printf("VT:%d R:%d W:%d | AT:%d R:%d W:%d \r",
           video_frame_count, video_read_idx, video_write_idx,
           audio_frame_count, audio_read_idx, audio_write_idx );
    fflush(stdout);
  } while (got_frame);

  printf("\n");

  if (video_dec_ctx)
    avcodec_close(video_dec_ctx);
  if (audio_dec_ctx)
    avcodec_close(audio_dec_ctx);
  avformat_close_input(&fmt_ctx);
  av_free(frame);

  // Free up buffer storage
  for ( i = 0; i < BUFFER_SIZE; i++ ) {
    free( video_storage[i] );
    free( audio_storage[i] );
  }

  return 0;
}
