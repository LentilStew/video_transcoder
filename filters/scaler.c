
#include "scaler.h"
#include "logger.h"

void resize_init(filters_path *filter_step)
{
    if (!filter_step->filter_params)
    {
        printf("Filter params have to be set");
        return;
    }

    scaler_params *params = (scaler_params *)filter_step->filter_params;

    filter_step->internal = sws_getContext(params->in_width, params->in_height, params->in_pixel_fmt,
                                           params->out_width, params->out_height, params->out_pixel_fmt,
                                           SWS_BILINEAR, NULL, NULL, NULL);
    return;
}

void resize_uninit(filters_path *filter_step)
{
    return;
}

AVFrame *resize_frame(filters_path *filter_step, AVFrame *frame)
{
    logging(INFO, "RESIZE FRAME: in width %i height %i pix_fmt %i", frame->width, frame->height, frame->format);

    int res;
    scaler_params *params = (scaler_params *)filter_step->filter_params;

    struct SwsContext *sws_ctx = filter_step->internal;

    AVFrame *new_frame = av_frame_alloc();
    new_frame->width = params->out_width;
    new_frame->height = params->out_height;
    new_frame->format = params->out_pixel_fmt;
    new_frame->pts = frame->pts;

    res = av_frame_get_buffer(new_frame, 0);
    if (res != 0)
    {
        return NULL;
    }
    sws_scale(sws_ctx, (const uint8_t **)frame->data, frame->linesize, 0, frame->height, new_frame->data, new_frame->linesize);

    logging(INFO, "RESIZE FRAME: out width %i height %i pix_fmt %i", frame->width, frame->height, frame->format);

    av_frame_free(&frame);

    return new_frame;
}

scaler_params *scaler_builder(int in_height, int in_width, int in_pixel_fmt, int out_height, int out_width, int out_pixel_fmt)
{
    scaler_params *params = malloc(sizeof(scaler_params));
    if (!params)
    {
        return NULL;
    }

    params->in_height = in_height;
    params->in_width = in_width;
    params->in_pixel_fmt = in_pixel_fmt;

    params->out_height = out_height;
    params->out_width = out_width;
    params->out_pixel_fmt = out_pixel_fmt;

    return params;
}

filters_path *build_resize_filter(int in_height, int in_width, int in_pixel_fmt,
                                  int out_height, int out_width, int out_pixel_fmt)
{
    logging(INFO, "CREATE FILTER RESIZE: in width %i height %i pix_fmt %i", in_width, in_height, in_pixel_fmt);
    logging(INFO, "CREATE FILTER RESIZE: out width %i height %i pix_fmt %i", out_width, out_height, out_pixel_fmt);

    filters_path *new = build_filters_path();
    if (!new)
    {
        logging(ERROR, "CREATE FILTER RESIZE: ERROR ALLOCATING RAM");

        return NULL;
    }
    new->filter_params = (scaler_params *)scaler_builder(in_height, in_width, in_pixel_fmt,
                                                         out_height, out_width, out_pixel_fmt);
    if (!new->filter_params)
    {
        logging(ERROR, "CREATE FILTER RESIZE: ERROR CREATING FILTER PARAMS");

        return NULL;
    }
    new->init = resize_init;
    new->filter_frame = resize_frame;
    new->uninit = resize_uninit;
    new->internal = NULL;
    new->next = NULL;
    new->multiple_output = 0;

    return new;
}