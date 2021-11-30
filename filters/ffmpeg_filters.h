#define _XOPEN_SOURCE 600 /* for usleep */
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "filter.h"
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>

typedef struct ffmpeg_filters_params
{
    AVRational time_base;
    int in_pix_fmt;
    int *out_pix_fmts; //AV_PIX_FMT_NONE terminated

    int in_width;
    int in_height;
    AVRational sample_aspect_ratio;
    const char *filters_descr;
    AVFilterContext *buffersink_ctx;
    AVFilterContext *buffersrc_ctx;

} ffmpeg_filters_params;

void init_ffmpeg_filter(filters_path *filter_step);

ffmpeg_filters_params *ffmpeg_filter_builder(const char *filters_descr, AVRational time_base, int in_pix_fmt,
                                             int *out_pix_fmts, int in_width, int in_height, AVRational sample_aspect_ratio);
                                             
void un_init_ffmpeg_filter(filters_path *filter_step);

AVFrame* apply_ffmpeg_filter(filters_path *filter_step, AVFrame *frame);

filters_path *build_ffmpeg_filter(const char *filters_descr, AVRational time_base, int in_pix_fmt,
                                  int *out_pix_fmts, int in_width, int in_height, AVRational sample_aspect_ratio);
