#include "filters/filter.h"
#include "filters/scaler.h"
#include "transcode_utils.h"
#include "filters/edges.h"
#include "filters/ffmpeg_filters.h"
#include "filters/audio_encoder.h"
#include "filters/video_encoder.h"
#include <unistd.h>
#include <threads.h>
#include "logger.h"

// If video decoder or audio decoder are NULL default ones are going to be used
file *create_decoder(char *path, const char *video_decoder, const char *audio_decoder);
int stream_clip(file *input, file *output);
file *start_output_from_file(const char *path, file *input, const char *video_encoder, const char *audio_encoder);
file *create_file(int streams, const char *filename);
filters_path *create_video_path(AVCodecContext *encoder, AVCodecContext *decoder, filters_path *path);
int times_per_second();

int main()
{
    int res;
    const char *output_path = "out.mp4";
    //https://clips-media-assets2.twitch.tv/AT-cm%7Csrf8WrWTR-507Wm4I2Wn2A.mp4
    file *input = create_decoder("in.mp4", "h264_cuvid", NULL);
    file *input2 = create_decoder("in.mp4", "h264_cuvid", NULL);

    if (!input || !input2)
    {
        logging(ERROR, "Failed creating input\n");
        return 1;
    }

    file *output = create_file(2, output_path);

    filters_path *video_root = NULL;
    filters_path *audio_root = NULL;

    int in_video_codec = find_codec(AVMEDIA_TYPE_VIDEO, input);
    int in_audio_codec = find_codec(AVMEDIA_TYPE_AUDIO, input);

    int in_video_stream = find_stream(AVMEDIA_TYPE_VIDEO, input);
    int in_audio_stream = find_stream(AVMEDIA_TYPE_AUDIO, input);
    if (!input)
    {
        printf("Failed opening input\n");
        return 1;
    }
    if (!output)
    {
        printf("failed creating output\n");
        return 1;
    }

    AVRational framerate = av_guess_frame_rate(input->container, input->container->streams[in_video_stream], NULL);

    input->paths[in_video_codec] = build_video_encoder(&output->codec[in_video_codec], output->container, "h264_nvenc", input->container->streams[in_video_stream]->codecpar->width,
                                                       input->container->streams[in_video_stream]->codecpar->height, 23, (AVRational){1, 1}, framerate, 45000000, 0, in_video_stream, input->container->streams[in_video_stream]->time_base);
    if (!input->paths[in_video_codec])
    {
        logging(ERROR, "MAIN: failed building video encoder");
        return 1;
    }
    init_path(input->paths[in_video_codec]);


    input->paths[in_audio_codec] = build_audio_encoder(&output->codec[in_audio_codec], output->container, "aac", 2,
                                                       44100, 123 * 1000, in_audio_codec, input->container->streams[in_video_stream]->time_base);
    if (!input->paths[in_audio_codec])
    {
        logging(ERROR, "MAIN: failed building audio encoder");
        return 1;
    }

    init_path(input->paths[in_audio_codec]);


    input2->paths = input->paths;

    if (output->container->oformat->flags & AVFMT_GLOBALHEADER)
        output->container->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if (!(output->container->oformat->flags & AVFMT_NOFILE))
    {
        if (avio_open(&output->container->pb, output_path, AVIO_FLAG_WRITE) < 0)
        {
            logging(ERROR, "could not open the output file");
            return 1;
        }
    }

    AVDictionary *muxer_opts = NULL;
    if (avformat_write_header(output->container, &muxer_opts) < 0)
    {
        logging(ERROR, "error opening output file");
        return 1;
    }

    stream_clip(input, output);
    stream_clip(input2, output);

    av_write_trailer(output->container);

    end_path(input->paths[in_audio_codec]);
    end_path(input->paths[in_video_codec]);

    free_file(input);
    free_file(output);
}

filters_path *create_video_path(AVCodecContext *encoder, AVCodecContext *decoder, filters_path *path)
{
    filters_path *root = NULL;

    if (encoder->width != decoder->width || encoder->height != decoder->height || decoder->pix_fmt != encoder->pix_fmt)
    {
        root = build_resize_filter(decoder->height, decoder->width, decoder->pix_fmt, encoder->height, encoder->width, encoder->pix_fmt);
        if (!root)
        {
            printf("Failed creating path\n");
            return NULL;
        }
    }

    return root;
}

file *create_file(int streams, const char *filename)
{
    int res;

    file *output = malloc(sizeof(file));
    if (!output)
    {
        return NULL;
    }

    output->nb_streams = streams;

    res = avformat_alloc_output_context2(&output->container, NULL, NULL, filename);
    if (res < 0)
    {
        printf("Failed opening output\n");
        return NULL;
    }

    output->codec = av_calloc(streams, sizeof(AVCodecContext *));
    output->paths = av_calloc(streams, sizeof(filters_path *));
    output->frames = malloc(2 * sizeof(int));

    if (!output->codec || !output->paths || !output->frames)
    {
        printf("Failed allocating ram for file\n");
        return NULL;
    }

    for (int stream = 0; stream < streams; stream++)
    {
        output->paths[stream] = NULL;
        output->codec[stream] = NULL;
        output->frames[stream] = 0;
    }

    return output;
}

// If video decoder or audio decoder are NULL default ones are going to be used
file *create_decoder(char *path, const char *video_decoder, const char *audio_decoder)
{
    file *input = malloc(sizeof(file));
    int res;

    res = open_file(input, path);

    if (res != 0)
    {
        logging(ERROR, "Failed opening file\n");

        return NULL;
    }

    res = open_codecs(input, video_decoder, audio_decoder);
    if (res != 0)
    {
        logging(ERROR, "Failed opening codecs\n");

        return NULL;
    }

    input->paths = av_calloc(input->container->nb_streams, sizeof(filters_path *));

    if (!input->paths)
    {
        logging(ERROR, "Failed allocating ram for file\n");
        return NULL;
    }

    for (int stream = 0; stream < input->container->nb_streams; stream++)
    {
        input->paths[stream] = NULL;
    }

    return input;
}
int frames = 0;
int stream_clip(file *input, file *output)
{
    AVFrame *frame = av_frame_alloc();
    AVPacket *packet = av_packet_alloc();
    int res;
    while (1)
    {
        res = decode_frame(input, frame, packet);

        if (res == 1)
        {
            logging(ERROR, "Error decoding a frame\n");

            av_frame_free(&frame);
            av_packet_free(&packet);

            return 1;
        }
        else if (res == 0)
        {
            if (packet->stream_index == 0)
            {
                printf("%i\n", frames);
                frames++;
            }
            apply_path(input->paths[packet->stream_index], frame);
        }
        else if (res == -1)
        {
            printf("File ended\n");
            break;
        }
    }

    av_frame_free(&frame);
    av_packet_free(&packet);

    return 0;
}

file *start_output_from_file(const char *path, file *input, const char *video_encoder, const char *audio_encoder)
{
    int res;

    file *output = create_file(input->container->nb_streams, path);
    if (!output)
    {
        return NULL;
    }
    AVCodecContext *codec_ctx;
    for (int stream = 0; stream < input->container->nb_streams; stream++)
    {
        codec_ctx = input->codec[stream];

        switch (codec_ctx->codec_type)
        {
        case AVMEDIA_TYPE_AUDIO:
            if (audio_encoder == NULL)
            {
                audio_encoder = codec_ctx->codec_descriptor->name;
            }
            res = create_audio_encoder(&output->codec[stream], output->container, audio_encoder, codec_ctx->channels, codec_ctx->sample_rate, codec_ctx->bit_rate);

            break;

        case AVMEDIA_TYPE_VIDEO:
            if (video_encoder == NULL)
            {
                video_encoder = codec_ctx->codec_descriptor->name;
            }
            AVRational framerate = av_guess_frame_rate(input->container, input->container->streams[stream], NULL);
            res = create_video_encoder(&output->codec[stream], output->container, video_encoder, codec_ctx->width, codec_ctx->height,
                                       AV_PIX_FMT_NV12, (AVRational){1, 1}, framerate, codec_ctx->bit_rate, codec_ctx->rc_buffer_size);
            break;
        }
        if (res != 0)
        {
            printf("Failed opening encoder stream number %i \n", stream);
            return NULL;
        }
    }

    if (output->container->oformat->flags & AVFMT_GLOBALHEADER)
        output->container->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if (!(output->container->oformat->flags & AVFMT_NOFILE))
    {
        if (avio_open(&output->container->pb, path, AVIO_FLAG_WRITE) < 0)
        {
            printf("could not open the output file");
            return NULL;
        }
    }

    AVDictionary *muxer_opts = NULL;

    if (avformat_write_header(output->container, &muxer_opts) < 0)
    {
        printf("an error occurred when opening output file");
        return NULL;
    }

    return output;
}
/*
 DEV.LS h264
decoders: h264 h264_v4l2m2m h264_cuvid 
encoders: libx264 libx264rgb h264_nvenc h264_v4l2m2m h264_vaapi 

DEV.L. hevc 

decoders: hevc hevc_v4l2m2m hevc_cuvid 
encoders: libx265 hevc_nvenc hevc_v4l2m2m hevc_vaapi 
*/

//input2->paths[in_video_codec2] = create_video_path(output->codec[out_video_codec], input2->codec[in_video_codec2], input2->paths[in_video_codec2]);

/*



    root->next = build_edges_filter(output_ctx->width, output_ctx->height);

    if (!root->next)
    {
        printf("Failed creating path\n");
        return 1;
    }

    root->next->next = build_resize_filter(output_ctx->height, output_ctx->width, AV_PIX_FMT_RGB24,
                                           output_ctx->height, output_ctx->width, output_ctx->pix_fmt);
    if (!root->next->next)
    {
        printf("Failed creating path\n");
        return 1;
    }

*/
/*
    filters_path *root = NULL;
    root = build_ffmpeg_filter("scale=1920x1080", input2->container->streams[in_video_codec2]->time_base, input_ctx->pix_fmt, pixs,
                               input2->container->streams[in_video_codec2]->codecpar->width, input2->container->streams[in_video_codec2]->codecpar->height, input_ctx->sample_aspect_ratio);
    if (!root)
    {
        printf("Failed creating path\n");
        return 1;
    }


*/