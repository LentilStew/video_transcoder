#include <libswscale/swscale.h>
#include <libavformat/avformat.h>

#include "filter.h"

typedef struct scaler_params
{
    int in_width;
    int in_height;
    int in_pixel_fmt;
    int out_width;
    int out_height;
    int out_pixel_fmt;
} scaler_params;

typedef struct internal_params
{
    struct SwsContext *scaler;
} internal_params;

filters_path *build_resize_filter(int in_width, int in_height, int in_pixel_fmt, int out_width, int out_height, int out_pixel_fmt);

scaler_params *scaler_builder(int in_width, int in_height, int in_pixel_fmt, int out_width, int out_height, int out_pixel_fmt);

AVFrame *resize_frame(filters_path *filter_step, AVFrame *oldFrame);

void resize_uninit(filters_path *filter_step);

void resize_init(filters_path *filter_step);