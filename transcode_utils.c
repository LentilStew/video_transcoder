#include "logger.h"
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/avutil.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "filters/filter.h"
#include "transcode_utils.h"
#include <stdbool.h>

int encode_frame(file *encoder, AVFrame *input_frame, int index)
{

    AVPacket *output_packet = av_packet_alloc();
    if (!output_packet)
    {
        logging(ERROR,"ENCODER: Failed mallocing output_package");
        return 1;
    }

    AVCodecContext *codec = encoder->codec[index];

    int response = avcodec_send_frame(codec, input_frame);

    while (response >= 0)
    {
        response = avcodec_receive_packet(codec, output_packet);

        if (response == AVERROR(EAGAIN) || response == AVERROR_EOF)
        {
            break;
        }
        else if (response < 0)
        {
            logging(ERROR,"ENCODER: Error receiving packet");

            return 1;
        }

        output_packet->stream_index = index;

        if (index == 1)
        {
            output_packet->dts = encoder->frames[index] * 1024;
            output_packet->pts = encoder->frames[index] * 1024;
            encoder->frames[index]++;
        }
        else
        {
            output_packet->dts = encoder->frames[index] * 512;
            output_packet->pts = encoder->frames[index] * 512;
            encoder->frames[index]++;
        }

        response = av_interleaved_write_frame(encoder->container, output_packet);

        if (response != 0)
        {
            logging(ERROR,"ENCODER:failed writing frame");

            return 1;
        }
    }
    av_packet_unref(output_packet);
    av_packet_free(&output_packet);

    return 0;
}

int create_video_encoder(AVCodecContext **cod_ctx, AVFormatContext *container, const char *encoder, int width, int height,
                         int pix_fmt, AVRational sample_aspect_ratio, AVRational frame_rate, int bit_rate, int buffer_size)
{
    AVStream *stream = avformat_new_stream(container, NULL);
    if (!stream)
    {
        logging(ERROR,"CREATE VIDEO ENCODER: Failed allocating memory for stream");
        return 1;
    }
    const AVCodec *enc = avcodec_find_encoder_by_name(encoder);
    if (!enc)
    {
        logging(ERROR,"CREATE VIDEO ENCODER: Failed searching encoder");

        return 1;
    }

    cod_ctx[0] = avcodec_alloc_context3(enc);

    if (!cod_ctx[0])
    {
        logging(ERROR,"CREATE VIDEO ENCODER: Failed allocation codec context");
        return 1;
    }

    cod_ctx[0]->height = height;
    cod_ctx[0]->width = width;
    cod_ctx[0]->pix_fmt = pix_fmt;

    cod_ctx[0]->sample_aspect_ratio = sample_aspect_ratio;
    cod_ctx[0]->time_base = av_inv_q(frame_rate); //av_inv_q(frame_rate);
    cod_ctx[0]->framerate = frame_rate;
    cod_ctx[0]->bit_rate = bit_rate;
    cod_ctx[0]->rc_buffer_size = buffer_size;
    cod_ctx[0]->rc_max_rate = buffer_size;
    cod_ctx[0]->rc_min_rate = buffer_size;

    stream->time_base = cod_ctx[0]->time_base; //cod_ctx[0]->time_base;

    int res = 0;

    res = av_opt_set(cod_ctx[0]->priv_data, "preset", "fast", 0);

    if (res != 0)
    {
        logging(ERROR,"CREATE VIDEO ENCODER: Failed opt set");
        return 1;
    }

    res = avcodec_open2(cod_ctx[0], enc, NULL);
    if (res < 0)
    {
        logging(ERROR,"CREATE VIDEO ENCODER: couldn't open codec");
        return 1;
    }

    res = avcodec_parameters_from_context(stream->codecpar, cod_ctx[0]);

    if (res < 0)
    {
        logging(ERROR,"CREATE VIDEO ENCODER: failed setting codec parameters from context");
        return 1;
    }

    return 0;
}

int create_audio_encoder(AVCodecContext **cod_ctx, AVFormatContext *container, const char *encoder,
                         int channels, int sample_rate, int bit_rate)
{
    AVStream *stream = avformat_new_stream(container, NULL);
    if (!stream)
    {
        logging(ERROR,"CREATE AUDIO ENCODER: Failed allocating memory for stream");
        return 1;
    }
    const AVCodec *enc = avcodec_find_encoder_by_name(encoder);
    if (!enc)
    {
        logging(ERROR,"CREATE AUDIO ENCODER: Failed searching encoder");

        return 1;
    }

    cod_ctx[0] = avcodec_alloc_context3(enc);

    if (!cod_ctx[0])
    {
        logging(ERROR,"CREATE AUDIO ENCODER: Failed allocation codec context");
        return 1;
    }

    cod_ctx[0]->channels = channels;
    cod_ctx[0]->channel_layout = av_get_default_channel_layout(channels);
    cod_ctx[0]->sample_rate = sample_rate;
    cod_ctx[0]->sample_fmt = *enc->sample_fmts;
    cod_ctx[0]->bit_rate = bit_rate;
    cod_ctx[0]->time_base = (AVRational){1, sample_rate};

    int res = 0;

    res = avcodec_open2(cod_ctx[0], enc, NULL);
    if (res < 0)
    {
        logging(ERROR,"CREATE AUDIO ENCODER: couldn't open codec");
        return 1;
    }

    res = avcodec_parameters_from_context(stream->codecpar, cod_ctx[0]);

    if (res < 0)
    {
        logging(ERROR,"CREATE AUDIO ENCODER: failed setting codec parameters from context");
        return 1;
    }

    return 0;
}

int free_file(file *f)
{
    int i;
    for (i = 0; i < f->container->nb_streams; i++)
    {
        avcodec_free_context(&f->codec[i]);
    }

    av_free(f->codec);

    avformat_close_input(&f->container);

    av_free(f);
}

int open_file(file *video, const char input_path[])
{
    video->container = avformat_alloc_context();

    if (!video->container)
    {
        logging(ERROR,"OPEN_FILE: Failed to alloc memory");
        return 1;
    }
    if (avformat_open_input(&video->container, input_path, NULL, NULL) != 0)
    {
        logging(ERROR,"OPEN_FILE: Failed to open input file (%s)",input_path);
        return 1;
    }
    if (avformat_find_stream_info(video->container, NULL) < 0)
    {
        logging(ERROR,"OPEN_FILE: Failed to open read stream info");
        return 1;
    }

    return 0;
}
// returns index of stream
// returns -1 if not found
int find_codec(int stream, file *f)
{
    for (int i = 0; i < f->container->nb_streams; i++)
    {
        if (!f->codec[i])
        {
            continue;
        }
        else if (f->codec[i]->codec_type == stream)
        {
            return i;
        }
    }
    return -1;
}

//open file before opening codec
//if codecs are NULL, the codecs are the default
int open_codecs(file *video, const char *video_codec, const char *audio_codec)
{

    video->codec = av_calloc(video->container->nb_streams, sizeof(*video->codec));
    video->nb_streams = video->container->nb_streams;
    for (unsigned int i = 0; i < video->container->nb_streams; i++)
    {
        const char *curr_codec;

        AVStream *stream = video->container->streams[i];
        const AVCodec *dec;
        AVCodecContext *codec_ctx;

        if (AVMEDIA_TYPE_VIDEO == stream->codecpar->codec_type)
        {
            curr_codec = video_codec;
        }
        else if (AVMEDIA_TYPE_AUDIO == stream->codecpar->codec_type)
        {
            curr_codec = audio_codec;
        }
        else
        {
            continue;
        }

        if (curr_codec == NULL)
            dec = avcodec_find_decoder(stream->codecpar->codec_id);

        else
            dec = avcodec_find_decoder_by_name(video_codec);

        if (!dec)
        {
            logging(ERROR,"DECODER CODECS: failed to find the codec");
            return 1;
        }

        codec_ctx = avcodec_alloc_context3(dec);
        if (!codec_ctx)
        {
            logging(ERROR,"DECODER CODECS: failed to alloc memory for codec context");
            return 1;
        }

        if (avcodec_parameters_to_context(codec_ctx, stream->codecpar) < 0)
        {
            logging(ERROR,"DECODER CODECS: failed to fill codec context");
            return 1;
        }

        if (avcodec_open2(codec_ctx, dec, NULL) < 0)
        {
            logging(ERROR,"DECODER CODECS: failed to open codec");
            return 1;
        }

        video->codec[i] = codec_ctx;
    }
    return 0;
}

/*
    returns:
    1 if error
    0 if success
    -1 if file ended
*/
int decode_frame(file *decoder, AVFrame *frame, AVPacket *packet)
{
    AVCodecContext *dec;

    while (true)
    {
        av_packet_unref(packet);
        if (av_read_frame(decoder->container, packet) < 0)
            break;

        int index = packet->stream_index;

        dec = decoder->codec[index];

        if (dec == NULL)
        {
            continue;
        }

        int response = avcodec_send_packet(dec, packet);

        if (response < 0)
        {
            logging(ERROR,"FRAME DECODER: Error while sending packet to decoder");
            return 1;
        }
        while (response >= 0)
        {
            response = avcodec_receive_frame(dec, frame);
            if (response == AVERROR(EAGAIN) || response == AVERROR_EOF)
            {

                break;
            }
            else if (response < 0)
            {
                logging(ERROR,"FRAME DECODER: Error while receiving frame from decoder\n");
                return 1;
            }
            if (response >= 0)
            {
                return 0;
            }
            av_frame_unref(frame);
        }
    }
    return -1;
}
