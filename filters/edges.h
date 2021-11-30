#pragma once
#include <libavformat/avformat.h>
#include "filter.h"

typedef struct edges_params
{
    int width;
    int height;
} edges_params;

AVFrame *apply_edges(filters_path *a, AVFrame *frame);

filters_path *build_edges_filter(int width, int height);

edges_params *edges_builder(int height, int width);

void uninit_edges(filters_path *filter_step);

void init_edges(filters_path *filter_step);