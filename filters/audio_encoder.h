#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include "logger.h"
#include "filter.h"

typedef struct audio_encoder_params
{
    AVCodecContext **cod_ctx;
    AVFormatContext *container;
    const char *encoder;
    int channels;
    int sample_rate;
    int bit_rate;
    AVPacket *packet;
    int frames;
    int index;
    double pts_real_time;
    AVRational in_time_base;
    AVStream *stream;

} audio_encoder_params;

filters_path *build_audio_encoder(AVCodecContext **cod_ctx, AVFormatContext *container,
                                  const char *encoder, int channels, int sample_rate,
                                  int bit_rate, int index, AVRational in_time_base);

audio_encoder_params *audio_encoder_builder(AVCodecContext **cod_ctx, AVFormatContext *container,
                                            const char *encoder, int channels, int sample_rate,
                                            int bit_rate, int index, AVRational in_time_base);

AVFrame *encode_audio_frame(filters_path *filter_props, AVFrame *frame);

void uninit_audio_encoder(filters_path *filter_props);

void init_audio_encoder(filters_path *filter_step);