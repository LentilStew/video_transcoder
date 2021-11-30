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

file *prepare_file(char *path);
int stream_clip(file *input, file *output);
file *start_output_from_file(const char *path, file *input, const char *video_encoder, const char *audio_encoder);
file *create_file(int streams, const char *filename);
filters_path *create_video_path(AVCodecContext *encoder, AVCodecContext *decoder, filters_path *path);
int times_per_second();
int main()
{
    int res;
    const char *output_path = "test.mp4";

    file *input = prepare_file("./in2.mp4");
    file *output = create_file(2, output_path);
    filters_path *video_root = NULL;
    filters_path *audio_root = NULL;

    int in_video_codec = find_codec(AVMEDIA_TYPE_VIDEO, input);
    int in_audio_codec = find_codec(AVMEDIA_TYPE_AUDIO, input);

    if (!input)
    {
        printf("Failed in opening input\n");
        return 1;
    }
    if (!output)
    {
        printf("failed creating output\n");
        return 1;
    }

    input->paths[in_video_codec] = build_video_encoder(output->codec[in_video_codec], output->container, "h264_nvenc", 1280,
                                                       720, 23, (AVRational){1, 1}, (AVRational){1, 24}, 45000000, 0);
    if (!input->paths[in_video_codec])
    {
        logging(ERROR, "MAIN: failed building video encoder");
        return 1;
    }
    if(output->codec[in_video_codec] == NULL)
    {
        printf("ASDASDASDA\n");
        return;
    }
    input->paths[in_audio_codec] = build_audio_encoder(&output->codec[in_audio_codec], output->container, "aac", 2,
                                                       48000, 123 * 1000);
    if (!input->paths[in_audio_codec])
    {
        logging(ERROR, "MAIN: failed building audio encoder");
        return 1;
    }

    if (output->container->oformat->flags & AVFMT_GLOBALHEADER)
        output->container->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if (!(output->container->oformat->flags & AVFMT_NOFILE))
    {
        if (avio_open(&output->container->pb, output_path, AVIO_FLAG_WRITE) < 0)
        {
            printf("could not open the output file");
            return 1;
        }
    }

    AVDictionary *muxer_opts = NULL;
    if (avformat_write_header(output->container, &muxer_opts) < 0)
    {
        printf("an error occurred when opening output file");
        return 1;
    }

    stream_clip(input, output);

    av_write_trailer(output->container);
    free_file(input);
    free_file(output);
}

/*
int main()
{
    int res;

    file *input2 = prepare_file("./in2.mp4");

    if (!input2)
    {
        printf("Failed in opening input\n");
        return 1;
    }

    const char *output_path = "test.mp4";

    file *output = create_file(2, output_path);

    if (!output)
    {
        printf("failed creating output\n");
        return 1;
    }

    res = create_video_encoder(&output->codec[0], output->container, "h264_nvenc", 1920, 1080,
                               AV_PIX_FMT_NV12, (AVRational){1, 1}, (AVRational){1, 60}, input2->codec[0]->bit_rate, input2->codec[0]->rc_buffer_size);
    if (res != 0)
    {
        printf("Failed creating video encoder\n");
        return 1;
    }
    
    res = create_audio_encoder(&output->codec[1], output->container, "aac", 2, 44100, input2->codec[1]->bit_rate);
    if (res != 0)
    {
        printf("Failed creating audio encoder\n");
        return 1;
    }

    if (output->container->oformat->flags & AVFMT_GLOBALHEADER)
        output->container->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if (!(output->container->oformat->flags & AVFMT_NOFILE))
    {
        if (avio_open(&output->container->pb, output_path, AVIO_FLAG_WRITE) < 0)
        {
            printf("could not open the output file");
            return 1;
        }
    }

    AVDictionary *muxer_opts = NULL;
    if (avformat_write_header(output->container, &muxer_opts) < 0)
    {
        printf("an error occurred when opening output file");
        return 1;
    }

    int out_video_codec = find_codec(AVMEDIA_TYPE_VIDEO, output);
    int in_video_codec2 = find_codec(AVMEDIA_TYPE_VIDEO, input2);

    AVCodecContext *input_ctx = input2->codec[in_video_codec2];
    AVCodecContext *output_ctx = output->codec[out_video_codec];

    int pixs[] = {output_ctx->sample_fmt, AV_PIX_FMT_NONE};
    
    filters_path *root = NULL;

    root = build_resize_filter(input_ctx->height, input_ctx->width, input_ctx->pix_fmt,
                               output_ctx->height, output_ctx->width, output_ctx->pix_fmt);
    if (!root)
    {
        printf("Failed creating path\n");
        return 1;
    }
    

    filters_path *root = NULL;
    root = build_ffmpeg_filter("framerate=45", input2->container->streams[in_video_codec2]->time_base, input_ctx->pix_fmt, pixs,
                               input2->container->streams[in_video_codec2]->codecpar->width, input2->container->streams[in_video_codec2]->codecpar->height, input_ctx->sample_aspect_ratio);
    if (!root)
    {
        printf("Failed creating path\n");
        return 1;
    }

    input2->paths[in_video_codec2] = root;

    init_path(input2->paths[in_video_codec2]);

    thrd_t thr;
    thrd_create(&thr, times_per_second, NULL);

    stream_clip(input2, output);

    av_write_trailer(output->container);

    free_file(input2);
    free_file(output);

    return 0;
}
*/
int fp2s = 0;
int times_per_second()
{
    while (1)
    {
        printf("fps:%i\n", fp2s / 1);
        fp2s = 0;
        sleep(1);
    }
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

file *prepare_file(char *path)
{
    file *input = malloc(sizeof(file));
    int res;

    res = open_file(input, path);

    if (res != 0)
    {
        printf("Failed opening file\n");

        return NULL;
    }
    char *video_codec = NULL;

    for (unsigned int i = 0; i < input->container->nb_streams; i++)
    {
        if (input->container->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO &&
            input->container->streams[i]->codecpar->format == AV_PIX_FMT_YUV420P)
        {
            video_codec = "h264_cuvid";
        }
    }

    res = open_codecs(input, video_codec, NULL);
    if (res != 0)
    {
        printf("Failed opening codecs\n");

        return NULL;
    }

    input->paths = av_calloc(input->container->nb_streams, sizeof(filters_path *));

    if (!input->paths)
    {
        printf("Failed allocating ram for file\n");
        return NULL;
    }

    for (int stream = 0; stream < input->container->nb_streams; stream++)
    {
        input->paths[stream] = NULL;
    }

    return input;
}

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
            printf("Error decoding a frame\n");
            return 1;
        }

        else if (res == 0)
        {
            if (!packet->stream_index == 0)
            {
                fp2s++;
            }

            frame = apply_path(input->paths[packet->stream_index], frame);

            encode_frame(output, frame, packet->stream_index);
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