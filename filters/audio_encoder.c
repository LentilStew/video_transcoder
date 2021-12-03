#include "audio_encoder.h"
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include "logger.h"
#include "filter.h"

void init_audio_encoder(filters_path *filter_step)
{
    audio_encoder_params *params = filter_step->filter_params;

    AVFormatContext *container = params->container;
    const char *encoder = params->encoder;
    int channels = params->channels;
    int sample_rate = params->sample_rate;
    int bit_rate = params->bit_rate;
    int res;
    AVPacket *packet;

    params->packet = av_packet_alloc();
    params->packet->dts = 0;
    if (!params->packet)
    {
        logging(ERROR, "CREATE VIDEO ENCODER: Failed allocating memory for packet");
        return;
    }

    params->stream = avformat_new_stream(container, NULL);
    if (!params->stream)
    {
        logging(ERROR, "CREATE AUDIO ENCODER: Failed allocating memory for stream");
        return;
    }
    const AVCodec *enc = avcodec_find_encoder_by_name(encoder);
    if (!enc)
    {
        logging(ERROR, "CREATE AUDIO ENCODER: Failed searching encoder");

        return;
    }

    params->cod_ctx[0] = avcodec_alloc_context3(enc);
    AVCodecContext *cod_ctx = params->cod_ctx[0];

    if (!cod_ctx)
    {
        logging(ERROR, "CREATE AUDIO ENCODER: Failed allocation codec context");
        return;
    }

    cod_ctx->channels = channels;
    cod_ctx->channel_layout = av_get_default_channel_layout(channels);
    cod_ctx->sample_rate = sample_rate;
    cod_ctx->sample_fmt = *enc->sample_fmts;
    cod_ctx->bit_rate = bit_rate;
    cod_ctx->time_base = (AVRational){1, sample_rate};

    res = avcodec_open2(cod_ctx, enc, NULL);
    if (res < 0)
    {
        logging(ERROR, "CREATE AUDIO ENCODER: couldn't open codec");
        return;
    }

    res = avcodec_parameters_from_context(params->stream->codecpar, cod_ctx);

    if (res < 0)
    {
        logging(ERROR, "CREATE AUDIO ENCODER: failed setting codec parameters from context");
        return;
    }

    return;
}
void uninit_audio_encoder(filters_path *filter_props)
{
    audio_encoder_params *params = filter_props->filter_params;

    avcodec_free_context(params->cod_ctx);
    av_packet_free(&params->packet);
    free(params);

    return;
}

AVFrame *encode_audio_frame(filters_path *filter_props, AVFrame *frame)
{
    audio_encoder_params *params = filter_props->filter_params;

    int response = avcodec_send_frame(*params->cod_ctx, frame);

    while (response >= 0)
    {
        response = avcodec_receive_packet(*params->cod_ctx, params->packet);

        if (response == AVERROR(EAGAIN) || response == AVERROR_EOF)
        {
            break;
        }
        else if (response < 0)
        {
            logging(ERROR, "AUDIO ENCODER: Error receiving packet");
            return NULL;
        }

        params->packet->stream_index = params->index;

        av_packet_rescale_ts(params->packet, params->stream->time_base, params->stream->time_base);
        response = av_interleaved_write_frame(params->container, params->packet);

        params->frames++;

        if (response != 0)
        {
            logging(ERROR, "AUDIO ENCODER:failed writing frame");
            return NULL;
        }
    }

    av_packet_unref(params->packet);

    return frame;
}
audio_encoder_params *audio_encoder_builder(AVCodecContext **cod_ctx, AVFormatContext *container,
                                            const char *encoder, int channels, int sample_rate,
                                            int bit_rate, int index, AVRational in_time_base)
{
    audio_encoder_params *params = malloc(sizeof(audio_encoder_params));
    if (!params)
    {
        return NULL;
    }
    params->bit_rate = bit_rate;
    params->cod_ctx = cod_ctx;
    params->container = container;
    params->encoder = encoder;
    params->frames = 0;
    params->channels = channels;
    params->sample_rate = sample_rate;
    params->packet;
    params->index = index;
    params->in_time_base = in_time_base;

    return params;
}

filters_path *build_audio_encoder(AVCodecContext **cod_ctx, AVFormatContext *container,
                                  const char *encoder, int channels, int sample_rate,
                                  int bit_rate, int index, AVRational in_time_base)

{
    filters_path *new = build_filters_path();
    if (!new)
    {
        logging(ERROR, "BUILD ENCODER:failed allocating ram for filter path");

        return NULL;
    }

    new->filter_params = audio_encoder_builder(cod_ctx, container, encoder, channels,
                                               sample_rate, bit_rate, index, in_time_base);
    if (!new->filter_params)
    {
        logging(ERROR, "BUILD ENCODER:failed allocating ram for filter params");
        return NULL;
    }

    new->init = init_audio_encoder;
    new->filter_frame = encode_audio_frame;
    new->uninit = uninit_audio_encoder;
    new->encoder_filter_path = NULL;
    new->internal = NULL;
    new->next = NULL;
    new->multiple_output = 0;
    return new;
}
