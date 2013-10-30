#include <stdio.h>
#include <string.h>

#include <libavcodec/avcodec.h>
#include <libavutil/mathematics.h>
#include <libavutil/imgutils.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>

#include <rejigger.h>

#define STREAM_FRAME_RATE 30 /* 30 images/s */
#define STREAM_PIX_FMT AV_PIX_FMT_YUVJ420P /* We only fill Y */
#define WIDTH 640
#define HEIGHT 896

static int qmax_constant = 0;

/* Add an output stream. */
static AVStream *add_stream(AVFormatContext *oc, AVCodec **codec,
                            enum AVCodecID codec_id)
{
  AVCodecContext *c;
  AVStream *st;

  /* find the encoder */
  *codec = avcodec_find_encoder(codec_id);
  if (!(*codec)) {
    fprintf(stderr, "Could not find encoder for '%s'\n",
            avcodec_get_name(codec_id));
    exit(1);
  }

  st = avformat_new_stream(oc, *codec);
  if (!st) {
    fprintf(stderr, "Could not allocate stream\n");
    exit(1);
  }
  st->id = oc->nb_streams-1;
  c = st->codec;

  switch ((*codec)->type) {
  case AVMEDIA_TYPE_AUDIO:
    c->sample_fmt  = AV_SAMPLE_FMT_U8;
    c->bit_rate    = 0;
    c->sample_rate = 614400; // Enough to hold 20kb coming out at 30 hz
    c->channels    = 1;
    break;

  case AVMEDIA_TYPE_VIDEO:
    c->codec_id = codec_id;

    c->bit_rate = 0;
    c->qmin = 0;
    c->qmax = qmax_constant;
    /* Resolution must be a multiple of two. */
    c->width    = WIDTH;
    c->height   = HEIGHT;
    /* timebase: This is the fundamental unit of time (in seconds) in terms
     * of which frame timestamps are represented. For fixed-fps content,
     * timebase should be 1/framerate and timestamp increments should be
     * identical to 1. */
    c->time_base.den = STREAM_FRAME_RATE;
    c->time_base.num = 1;
    c->gop_size      = 12; /* emit one intra frame every twelve frames at most */
    c->pix_fmt       = STREAM_PIX_FMT;
    if (c->codec_id == AV_CODEC_ID_MPEG2VIDEO) {
      /* just for testing, we also add B frames */
      c->max_b_frames = 2;
    }
    if (c->codec_id == AV_CODEC_ID_MPEG1VIDEO) {
      /* Needed to avoid using macroblocks in which some coeffs overflow.
       * This does not happen with normal video, it just happens here as
       * the motion of the chroma plane does not match the luma plane. */
      c->mb_decision = 2;
    }
    av_opt_set(c->priv_data, "preset", "veryslow", 0 );
    break;
  default:
    break;
  }

  /* Some formats want stream headers to be separate. */
  if (oc->oformat->flags & AVFMT_GLOBALHEADER)
    c->flags |= CODEC_FLAG_GLOBAL_HEADER;

  return st;
}

int main( int argc, char ** argv ) {
  int i, got_frame, got_packet, ret, got_av_output;

  // Used by AVFormat to load up our input stream
  AVOutputFormat *out_fmt;
  AVFormatContext *in_fmt_ctx = NULL, *out_fmt_ctx = NULL;

  // Input Video storage
  int in_video_stream_idx = -1;
  int in_video_frame_count = 0;
  static uint8_t *in_video_dst_data[4] = {NULL};
  static int      in_video_dst_linesize[4];
  static int      in_video_dst_bufsize;
  AVCodec *in_video_codec;
  AVStream *in_video_st = NULL;
  AVCodecContext *in_video_codec_ctx = NULL;

  // Output storage infomration
  AVStream *out_audio_st = NULL, *out_video_st = NULL;
  AVCodec  *out_audio_codec, *out_video_codec;

  // Used by all
  AVFrame *in_frame = NULL, *out_frame = NULL;
  AVPacket in_pkt;
  AVCodecContext *codec_ctx = NULL;
  uint8_t **samples_data;
  int samples_linesize;
  AVDictionary *dictionary = NULL;

  static const char output_name[] = "test.mkv";

  // Check the input
  if ( argc != 2 && argc != 3 ) {
    fprintf(stderr,"Missing input argument\n\t%s <input pgms,blah%%07d.pgm> <optional qmax number>\n",
            argv[0] );
    exit(1);
  }

  if ( argc == 3 ) {
    sscanf(argv[2],"%d",&qmax_constant);
  }

  // Register all formats and codecs
  av_register_all();

  // OPENING INPUT
  // -----------------------------------------------------------------

  // Open input file ... which is actually a stream of PGM files. This
  // should later be something that the user can provide.
  if (avformat_open_input(&in_fmt_ctx, argv[1], NULL, NULL) < 0) {
    fprintf(stderr, "Couldn't open input source");
    exit(1);
  }

  // Retrieve stream information
  if (avformat_find_stream_info(in_fmt_ctx, NULL) < 0) {
    fprintf(stderr, "Could not find stream information\n");
    exit(1);
  }

  // Find Input Video Codec Stream Index
  if ((in_video_stream_idx = av_find_best_stream(in_fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0)) < 0) {
    fprintf(stderr, "Could not find video stream in input file.");
    exit(1);
  }

  // Find the decoder for this stream and open it
  in_video_st = in_fmt_ctx->streams[in_video_stream_idx];
  in_video_codec_ctx = in_video_st->codec;
  in_video_codec = avcodec_find_decoder(in_video_codec_ctx->codec_id);
  if ( !in_video_codec ) {
    fprintf(stderr, "Failed to find video codec\n");
    exit(1);
  }

  if (avcodec_open2(in_video_codec_ctx, in_video_codec, NULL) < 0) {
    fprintf(stderr, "Failed to open video codec\n");
    exit(1);
  }
  printf("Input pixel format is '%s'\n", av_get_pix_fmt_name(in_video_codec_ctx->pix_fmt) );

  // OPENING OUTPUT
  // -----------------------------------------------------------------
  avformat_alloc_output_context2(&out_fmt_ctx, NULL, "matroska", output_name );
  if ( !out_fmt_ctx ) {
    fprintf(stderr, "Faiil to create output context\n");
    exit(1);
  }
  out_fmt = out_fmt_ctx->oformat;
  out_fmt->audio_codec = AV_CODEC_ID_PCM_U8; // Uncompressed 8 bit audio

  // Attach output video stream
  out_video_st = add_stream( out_fmt_ctx, &out_video_codec, out_fmt->video_codec );

  // Attach output audio stream
  out_audio_st = add_stream( out_fmt_ctx, &out_audio_codec, out_fmt->audio_codec );

  // Open Video
  av_dict_set(&dictionary, "crf", "100", 0);
  av_dict_set(&dictionary, "threads", "auto", 0);
  codec_ctx = out_video_st->codec;
  ret = avcodec_open2(codec_ctx, out_video_codec, &dictionary );
  if ( ret < 0 ) {
    fprintf(stderr, "Could not open video codec: %s\n", av_err2str(ret) );
    exit(1);
  }

  // Allocate resusable output frame
  out_frame = avcodec_alloc_frame();
  if ( !out_frame ) {
    fprintf(stderr, "Could not allocate video frame\n");
    exit(1);
  }

  // Allocate the encoded raw picture
  ret = av_image_alloc(out_frame->data, out_frame->linesize, WIDTH, HEIGHT, STREAM_PIX_FMT, 32 );
  if ( ret < 0 ) {
    fprintf(stderr, "Could not allocate picture: %s\n", av_err2str(ret) );
    exit(1);
  }

  // Set UV to 128 sense we are a grayscale image
  for ( i = 0; i < HEIGHT/2; i++ ) {
    memset(out_frame->data[1]+out_frame->linesize[1]*i, 128, WIDTH/2);
    memset(out_frame->data[2]+out_frame->linesize[2]*i, 128, WIDTH/2);
  }

  // Open Audio
  codec_ctx = out_audio_st->codec;
  ret = avcodec_open2(codec_ctx, out_audio_codec, NULL );
  if ( ret < 0 ) {
    fprintf(stderr, "Could not open audio codec: %s\n", av_err2str(ret) );
    exit(1);
  }

  // Allocate space for samples
  ret = av_samples_alloc_array_and_samples(&samples_data, &samples_linesize, codec_ctx->channels,
                                           20 * 1024, codec_ctx->sample_fmt, 0 );
  if ( ret < 0 ) {
    fprintf(stderr, "Could not allocate source samples\n" );
    exit(1);
  }
  printf("Buffer size is %d\n",
         av_samples_get_buffer_size(&samples_linesize,1, 20 * 1024, codec_ctx->sample_fmt, 0 ) );

  // Initialize the output codecs for audio and video
  printf("Recommended video codec name is %s\n", avcodec_get_name(out_fmt->video_codec) );
  printf("Recommended audio codec name is %s\n", avcodec_get_name(out_fmt->audio_codec) );
  printf("Long name of format '%s'\n", out_fmt->long_name );
  printf("Acceptable extensions '%s'\n", out_fmt->extensions );

  av_dump_format( out_fmt_ctx, 0, output_name, 1 );

  // PERFORM PROCESSING
  // -----------------------------------------------------------------

  // Allocate space for our video frames we are reading
  in_video_dst_bufsize =
    av_image_alloc(in_video_dst_data, in_video_dst_linesize,
                   in_video_codec_ctx->width, in_video_codec_ctx->height,
                   in_video_codec_ctx->pix_fmt, 1 );

  // Allocate space for frame
  in_frame = avcodec_alloc_frame();
  if (!in_frame) {
    fprintf(stderr, "Could not allocate frame\n");
    exit(1);
  }

  // Initialize packet, set data to NULL so demuxer fills it in
  av_init_packet(&in_pkt);
  in_pkt.data = NULL;
  in_pkt.size = 0;

  // OPen the output file, if needed
  ret = avio_open(&out_fmt_ctx->pb, output_name, AVIO_FLAG_WRITE );
  if ( ret < 0 ) {
    fprintf(stderr, "Error occurred when opening the output file: %s\n",
            av_err2str(ret));
  }

  // Write the stream header
  ret = avformat_write_header(out_fmt_ctx, NULL);
  if ( ret < 0 ) {
    fprintf(stderr, "Error occurred when opening output file: %s\n",
            av_err2str(ret));
    exit(1);
  }

  if (out_frame)
    out_frame->pts = 0;
  while (av_read_frame(in_fmt_ctx, &in_pkt) >= 0) {

    // Decode video
    if (in_pkt.stream_index == in_video_stream_idx ) {
      if ( avcodec_decode_video2(in_video_codec_ctx, in_frame, &got_frame, &in_pkt) <= 0 ) {
        fprintf(stderr, "Error decoding video frame\n");
        exit(1);
      }

      if (got_frame) {
        /* // Debug ... write out frame */
        /* char filename[128]; */
        /* snprintf(filename,128,"test%i.pgm", in_video_frame_count); */
        /* pgm_save(frame->data[0], frame->linesize[0], */
        /*          in_video_codec_ctx->width, */
        /*          in_video_codec_ctx->height, filename ); */
        rejigger_small_frame( in_frame->data[0], in_frame->linesize[0],
                              out_frame->data[0], out_frame->linesize[0] );

        { // Write video frame
          AVPacket out_pkt = {0};
          av_init_packet(&out_pkt);
          ret = avcodec_encode_video2(out_video_st->codec, &out_pkt, out_frame, &got_av_output);
          if ( ret < 0 ) {
            fprintf(stderr, "Error encoding video frame: %s\n", av_err2str(ret));
            exit(1);
          }
          out_frame->pts += av_rescale_q(1, out_video_st->codec->time_base, out_video_st->time_base);

          // Size zero means image was buffered for p or b frames
          if ( !ret && got_av_output && out_pkt.size ) {
            out_pkt.stream_index = out_video_st->index;
            ret = av_interleaved_write_frame(out_fmt_ctx, &out_pkt);
            if ( ret != 0 ) {
              fprintf(stderr, "Error while writing video frame: %s\n", av_err2str(ret));
              exit(1);
            }
          }
        }

        { // Write audio packet
          AVPacket out_pkt = { 0 };
          AVFrame *aframe = avcodec_alloc_frame();
          aframe->pts = out_frame->pts;
          av_init_packet(&out_pkt);
          codec_ctx = out_audio_st->codec;
          for (i = 0; i < 32; i++ ) {
            memcpy(samples_data[0]+i*640,in_frame->data[0] + i * in_frame->linesize[0], 640);
          }
          aframe->nb_samples = 32 * 640;
          avcodec_fill_audio_frame(aframe, codec_ctx->channels,
                                   codec_ctx->sample_fmt, samples_data[0], samples_linesize, 0 );
          ret = avcodec_encode_audio2(codec_ctx, &out_pkt, aframe, &got_packet);
          if ( ret < 0 ) {
            fprintf(stderr, "Error encoding audio frame: %s\n", av_err2str(ret));
            exit(1);
          }

          if (got_packet) {
            out_pkt.stream_index = out_audio_st->index;
            ret = av_interleaved_write_frame(out_fmt_ctx, &out_pkt);
            if ( ret != 0 ) {
              fprintf(stderr, "Error while writing audio frame: %s\n", av_err2str(ret));
              exit(1);
            }
          } else {
            fprintf(stderr, "Didn't get audio packet?\n");
          }
          avcodec_free_frame(&aframe);
        }

        printf("in_video_frame n:%d coded_n:%d pts:%s\r",
               in_video_frame_count++, out_frame->coded_picture_number,
               av_ts2timestr(out_frame->pts, &codec_ctx->time_base));
        fflush(stdout);
      }
    }

    av_free_packet(&in_pkt);
  }

  // finish up the delayed frames
  for ( got_av_output = 1; got_av_output;  ) {
    AVPacket out_pkt = {0};
    av_init_packet(&out_pkt);
    ret = avcodec_encode_video2(out_video_st->codec, &out_pkt, NULL, &got_av_output);
    if ( ret < 0 ) {
      fprintf(stderr, "Error encoding frame\n");
      exit(1);
    }
    out_frame->pts += av_rescale_q(1, out_video_st->codec->time_base, out_video_st->time_base);


    if ( got_av_output ) {
      out_pkt.stream_index = out_video_st->index;
      ret = av_interleaved_write_frame(out_fmt_ctx, &out_pkt);
      if ( ret != 0 ) {
        fprintf(stderr, "Error while writing video frame: %s\n", av_err2str(ret));
        exit(1);
      }
      printf("Wrote another packet\n");
    }
  }

  av_write_trailer(out_fmt_ctx);
  printf("\n");

  // Kill everything
  if (in_video_codec_ctx)
    avcodec_close(in_video_codec_ctx);
  avformat_close_input(&in_fmt_ctx);
  av_free(in_frame);
  av_free(in_video_dst_data[0]);
  // Close output video
  avcodec_close(out_video_st->codec);
  av_free(out_frame);
  // Close output audio
  avcodec_close(out_audio_st->codec);
  av_free(samples_data[0]);
  // Close output file
  avio_close(out_fmt_ctx->pb);
  // Close the output file context
  avformat_free_context(out_fmt_ctx);
  // Kill the dictionary
  av_dict_free(&dictionary);
  return 0;
}
