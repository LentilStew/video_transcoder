#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include "logger.h"
#include "filter.h"

typedef struct video_encoder_params
{
    AVCodecContext **cod_ctx;
    AVFormatContext *container;
    const char *encoder;
    int width;
    int height;
    int pix_fmt;
    int frames;
    AVRational sample_aspect_ratio;
    AVRational frame_rate;
    int index;
    double pts_real_time;
    int bit_rate;
    int buffer_size;
    AVPacket *packet;
    AVRational in_time_base;
    AVStream* stream;
} video_encoder_params;

filters_path *build_video_encoder(AVCodecContext **cod_ctx, AVFormatContext *container, const char *encoder,
                                  int width, int height, int pix_fmt, AVRational sample_aspect_ratio,
                                  AVRational frame_rate, int bit_rate, int buffer_size, int index,
                                  AVRational in_time_base);

video_encoder_params *video_encoder_builder(AVCodecContext **cod_ctx, AVFormatContext *container, const char *encoder,
                                            int width, int height, int pix_fmt, AVRational sample_aspect_ratio,
                                            AVRational frame_rate, int bit_rate, int buffer_size, int index,
                                            AVRational in_time_base);

AVFrame *encode_video_frame(filters_path *filter_props, AVFrame *frame);

void uninit_video_encoder(filters_path *filter_props);

void init_video_encoder(filters_path *filter_step);