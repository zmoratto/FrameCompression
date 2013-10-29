#include <libavformat/avformat.h>

#include <rejigger.h>

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
  int ret = 0, got_frame;
  AVFormatContext *fmt_ctx = NULL;
  AVCodecContext *video_dec_ctx = NULL, *audio_dec_ctx = NULL;
  AVStream *video_stream = NULL, *audio_stream = NULL;

  int video_stream_idx = -1;
  int audio_stream_idx = -1;
  AVFrame *frame = NULL;
  AVPacket pkt;
  int video_frame_count = 0;
  int audio_frame_count = 0;

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

    /* /\* allocate image where the decoded image will be put *\/ */
    /* ret = av_image_alloc(video_dst_data, video_dst_linesize, */
    /*                      video_dec_ctx->width, video_dec_ctx->height, */
    /*                      video_dec_ctx->pix_fmt, 1); */
    /* if (ret < 0) { */
    /*   fprintf(stderr, "Could not allocate raw video buffer\n"); */
    /*   return 1; */
    /* } */
    /* video_dst_bufsize = ret; */
  }

  if (open_codec_context(&audio_stream_idx, fmt_ctx, AVMEDIA_TYPE_AUDIO) >= 0) {
    int nb_planes;

    audio_stream = fmt_ctx->streams[audio_stream_idx];
    audio_dec_ctx = audio_stream->codec;

    /* nb_planes = av_sample_fmt_is_planar(audio_dec_ctx->sample_fmt) ? */
    /*   audio_dec_ctx->channels : 1; */
    /* audio_dst_data = av_mallocz(sizeof(uint8_t *) * nb_planes); */
    /* if (!audio_dst_data) { */
    /*   fprintf(stderr, "Could not allocate audio data buffers\n"); */
    /*   ret = AVERROR(ENOMEM); */
    /*   return 1; */
    /* } */
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
    } else if ( pkt.stream_index == audio_stream_idx ) {
      audio_frame_count++;
    }
    printf("Video:%d Audio:%d\n",video_frame_count,audio_frame_count);

    av_free_packet(&pkt);
  }

  if (video_dec_ctx)
    avcodec_close(video_dec_ctx);
  if (audio_dec_ctx)
    avcodec_close(audio_dec_ctx);
  avformat_close_input(&fmt_ctx);
  av_free(frame);

  return 0;
}
