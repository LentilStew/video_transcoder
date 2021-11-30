#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include "logger.h"
#include "filter.h"

typedef struct audio_encoder_params
{
    AVCodecContext *cod_ctx;
    AVFormatContext *container;
    const char *encoder;
    int channels;
    int sample_rate;
    int bit_rate;
    AVPacket *packet;
    int frames;

} audio_encoder_params;
filters_path *build_audio_encoder(AVCodecContext *cod_ctx, AVFormatContext *container,
                                  const char *encoder, int channels, int sample_rate,
                                  int bit_rate);

audio_encoder_params *audio_encoder_builder(AVCodecContext *cod_ctx, AVFormatContext *container,
                                            const char *encoder, int channels, int sample_rate,
                                            int bit_rate);

AVFrame *encode_audio_frame(filters_path *filter_props, AVFrame *frame);

void uninit_audio_encoder(filters_path *filter_props);

void init_audio_encoder(filters_path *filter_step);