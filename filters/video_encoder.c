#include "video_encoder.h"
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include "logger.h"
#include "filter.h"

void init_video_encoder(filters_path *filter_step)
{
    video_encoder_params *params = filter_step->filter_params;
    AVCodecContext *cod_ctx = params->cod_ctx;
    AVFormatContext *container = params->container;
    const char *encoder = params->encoder;
    int width = params->width;
    int height = params->height;
    int pix_fmt = params->pix_fmt;
    AVRational sample_aspect_ratio = params->sample_aspect_ratio;
    AVRational frame_rate = params->frame_rate;
    int bit_rate = params->bit_rate;
    int buffer_size = params->buffer_size;
    int res;

    params->packet = av_packet_alloc();
    if (!params->packet)
    {
        logging(ERROR, "CREATE VIDEO ENCODER: Failed allocating memory for packet");
        return;
    }

    AVStream *stream = avformat_new_stream(container, NULL);
    if (!stream)
    {
        logging(ERROR, "CREATE VIDEO ENCODER: Failed allocating memory for stream");
        return;
    }
    const AVCodec *enc = avcodec_find_encoder_by_name(encoder);
    if (!enc)
    {
        logging(ERROR, "CREATE VIDEO ENCODER: Failed searching encoder");

        return;
    }

    cod_ctx = avcodec_alloc_context3(enc);

    if (!cod_ctx)
    {
        logging(ERROR, "CREATE VIDEO ENCODER: Failed allocation codec context");
        return;
    }

    cod_ctx->height = height;
    cod_ctx->width = width;
    cod_ctx->pix_fmt = pix_fmt;
    cod_ctx->sample_aspect_ratio = sample_aspect_ratio;
    cod_ctx->time_base = av_inv_q(frame_rate); //av_inv_q(frame_rate);
    cod_ctx->framerate = frame_rate;
    cod_ctx->bit_rate = bit_rate;
    cod_ctx->rc_buffer_size = buffer_size;
    cod_ctx->rc_max_rate = buffer_size;
    cod_ctx->rc_min_rate = buffer_size;

    stream->time_base = cod_ctx->time_base; //cod_ctx[0]->time_base;

    res = av_opt_set(cod_ctx->priv_data, "preset", "fast", 0);
    if (res != 0)
    {
        logging(ERROR, "CREATE VIDEO ENCODER: Failed opt set");
        return;
    }

    res = avcodec_open2(cod_ctx, enc, NULL);
    if (res < 0)
    {
        logging(ERROR, "CREATE VIDEO ENCODER: couldn't open codec");
        return;
    }

    res = avcodec_parameters_from_context(stream->codecpar, cod_ctx);
    if (res < 0)
    {
        logging(ERROR, "CREATE VIDEO ENCODER: failed setting codec parameters from context");
        return;
    }
    return;
}

void uninit_video_encoder(filters_path *filter_props)
{
    video_encoder_params *params = filter_props->filter_params;

    avcodec_free_context(&params->cod_ctx);
    av_packet_free(&params->packet);
    free(params);

    return;
}

AVFrame *encode_video_frame(filters_path *filter_props, AVFrame *frame)
{
    video_encoder_params *params = filter_props->filter_params;

    int response = avcodec_send_frame(params->cod_ctx, frame);

    while (response >= 0)
    {
        response = avcodec_receive_packet(params->cod_ctx, params->packet);

        if (response == AVERROR(EAGAIN) || response == AVERROR_EOF)
        {
            break;
        }
        else if (response < 0)
        {
            logging(ERROR, "ENCODER: Error receiving packet");
            return NULL;
        }

        params->packet->dts = params->frames * 1024;
        params->packet->pts = params->frames * 1024;
        params->frames++;

        response = av_interleaved_write_frame(params->container, params->packet);

        if (response != 0)
        {
            logging(ERROR, "ENCODER:failed writing frame");
            return NULL;
        }
    }

    av_packet_unref(params->packet);

    return frame;
}
video_encoder_params *video_encoder_builder(AVCodecContext *cod_ctx, AVFormatContext *container, const char *encoder,
                                            int width, int height, int pix_fmt, AVRational sample_aspect_ratio,
                                            AVRational frame_rate, int bit_rate, int buffer_size)
{
    video_encoder_params *params = malloc(sizeof(video_encoder_params));
    if (!params)
    {
        return NULL;
    }
    params->height = height;
    params->width = width;
    params->bit_rate = bit_rate;
    params->buffer_size = buffer_size;
    params->cod_ctx = cod_ctx;
    params->container = container;
    params->encoder = encoder;
    params->frame_rate = frame_rate;
    params->pix_fmt = pix_fmt;
    params->sample_aspect_ratio = sample_aspect_ratio;
    params->frames = 0;

    return params;
}

filters_path *build_video_encoder(AVCodecContext *cod_ctx, AVFormatContext *container, const char *encoder,
                            int width, int height, int pix_fmt, AVRational sample_aspect_ratio,
                            AVRational frame_rate, int bit_rate, int buffer_size)
{
    filters_path *new = malloc(sizeof(filters_path));
    if (!new)
    {
        logging(ERROR, "BUILD ENCODER:failed allocating ram for filter path");

        return NULL;
    }

    new->filter_params = video_encoder_builder(cod_ctx, container, encoder, width,
                                               height, pix_fmt, sample_aspect_ratio,
                                               frame_rate, bit_rate, buffer_size);
    if (!new->filter_params)
    {
        logging(ERROR, "BUILD ENCODER:failed allocating ram for filter params");
        return NULL;
    }

    new->init = init_video_encoder;
    new->filter_frame = encode_video_frame;
    new->uninit = uninit_video_encoder;
    new->internal = NULL;
    new->next = NULL;
    new->multiple_output = 0;
    return new;
}