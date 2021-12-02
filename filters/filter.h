
#pragma once
#include <libavformat/avformat.h>

typedef struct filters_path
{
    void (*init)(struct filters_path *ctx);

    AVFrame *(*filter_frame)(struct filters_path *ctx, AVFrame *frame);

    void (*uninit)(struct filters_path *ctx);

    void *filter_params;
    void *internal;

    int multiple_output; // 0 no ; 1 yes

    struct filters_path *encoder_filter_path;

    struct filters_path *next;

} filters_path;

void apply_path(filters_path *path, AVFrame *frame);

filters_path *build_filters_path();

void init_path(filters_path *path);

void uninit_path(filters_path *path);

void free_filter_path(filters_path *f);