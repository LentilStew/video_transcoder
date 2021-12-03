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

typedef struct file
{
    AVFormatContext *container;
    AVCodecContext **codec;
    filters_path **paths;
    int *frames;
    int nb_streams;
} file;

int encode_frame(file *encoder, AVFrame *input_frame, int index);

int find_codec(int stream, file *f);

int create_video_encoder(AVCodecContext **cod_ctx, AVFormatContext *container, const char *encoder, int width, int height,
                         int pix_fmt, AVRational sample_aspect_ratio, AVRational frame_rate,
                         int bit_rate, int buffer_size);

int create_audio_encoder(AVCodecContext **cod_ctx, AVFormatContext *container, const char *encoder,
                         int channels, int sample_rate, int bit_rate);

int find_stream(int stream, file *f);

int open_file(file *video, const char input_path[]);

int decode_frame(file *decoder, AVFrame *frame, AVPacket *packet);

int open_codecs(file *video, const char *video_codec, const char *audio_codec);

int free_file(file *f);
