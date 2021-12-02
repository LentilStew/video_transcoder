#include "ffmpeg_filters.h"

void init_ffmpeg_filter(filters_path *filter_step)
{
    ffmpeg_filters_params *ffmpeg_params = (ffmpeg_filters_params *)filter_step->filter_params;
    char args[512];
    int ret = 0;
    AVFilterContext *buffersink_ctx;
    AVFilterContext *buffersrc_ctx;
    const AVFilter *buffersrc = avfilter_get_by_name("buffer");
    const AVFilter *buffersink = avfilter_get_by_name("buffersink");

    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs = avfilter_inout_alloc();
    AVFilterGraph *filter_graph = avfilter_graph_alloc();
    if (!outputs || !inputs || !filter_graph)
    {
        ret = AVERROR(ENOMEM);
        return;
    }

    AVRational time_base = ffmpeg_params->time_base;

    /* buffer video source: the decoded frames from the decoder will be inserted here. */
    snprintf(args, sizeof(args),
             "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
             ffmpeg_params->in_width, ffmpeg_params->in_height, ffmpeg_params->in_pix_fmt,
             ffmpeg_params->time_base.num, ffmpeg_params->time_base.den,
             ffmpeg_params->sample_aspect_ratio.num, ffmpeg_params->sample_aspect_ratio.den);

    ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
                                       args, NULL, filter_graph);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Cannot create buffer source\n");
        return;
    }

    /* buffer video sink: to terminate the filter chain. */
    ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
                                       NULL, NULL, filter_graph);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Cannot create buffer sink\n");
        return;
    }

    ret = av_opt_set_int_list(buffersink_ctx, "pix_fmts", ffmpeg_params->out_pix_fmts,
                              AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Cannot set output pixel format\n");
        return;
    }

    /*
     * Set the endpoints for the filter graph. The filter_graph will
     * be linked to the graph described by filters_descr.
     */

    /*
     * The buffer source output must be connected to the input pad of
     * the first filter described by filters_descr; since the first
     * filter input label is not specified, it is set to "in" by
     * default.
     */
    outputs->name = av_strdup("in");
    outputs->filter_ctx = buffersrc_ctx;
    outputs->pad_idx = 0;
    outputs->next = NULL;

    /*
     * The buffer sink input must be connected to the output pad of
     * the last filter described by filters_descr; since the last
     * filter output label is not specified, it is set to "out" by
     * default.
     */
    inputs->name = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx = 0;
    inputs->next = NULL;

    if ((ret = avfilter_graph_parse_ptr(filter_graph, ffmpeg_params->filters_descr,
                                        &inputs, &outputs, NULL)) < 0)
        return;

    if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0)
        return;

    ffmpeg_params->buffersrc_ctx = buffersrc_ctx;
    ffmpeg_params->buffersink_ctx = buffersink_ctx;

end:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);

    return;
}

AVFrame *apply_ffmpeg_filter(filters_path *filter_step, AVFrame *frame)
{
    /* push the decoded frame into the filtergraph */
    AVFrame *new_frame = av_frame_alloc();

    int res;

    ffmpeg_filters_params *params = filter_step->filter_params;

    if (av_buffersrc_add_frame_flags(params->buffersrc_ctx, frame, AV_BUFFERSRC_FLAG_KEEP_REF) < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Error while feeding the filtergraph\n");

        return NULL;
    }

    /* pull filtered frames from the filtergraph */
    while (1)
    {

        res = av_buffersink_get_frame(params->buffersink_ctx, new_frame);
        int *a;

        if (res == AVERROR(EAGAIN) || res == AVERROR_EOF)
        {
            av_frame_free(&frame);
            return NULL;
        }

        if (res < 0)
            return NULL;

        return new_frame;
    }
}

void un_init_ffmpeg_filter(filters_path *filter_step)
{
    return;
}
/*
typedef struct ffmpeg_filters_params
{
    const char *filters_descr;
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
*/
ffmpeg_filters_params *ffmpeg_filter_builder(const char *filters_descr, AVRational time_base, int in_pix_fmt,
                                             int *out_pix_fmts, int in_width, int in_height, AVRational sample_aspect_ratio)
{
    ffmpeg_filters_params *new = malloc(sizeof(ffmpeg_filters_params));

    if (!new)
    {
        return NULL;
    }
    new->buffersink_ctx = NULL;
    new->buffersrc_ctx = NULL;
    new->filters_descr = filters_descr;
    new->in_height = in_height;
    new->in_width = in_width;
    new->sample_aspect_ratio = sample_aspect_ratio;
    new->time_base = time_base;
    new->out_pix_fmts = out_pix_fmts;
    new->in_pix_fmt = in_pix_fmt;
    return new;
}

filters_path *build_ffmpeg_filter(const char *filters_descr, AVRational time_base, int in_pix_fmt,
                                  int *out_pix_fmts, int in_width, int in_height, AVRational sample_aspect_ratio)
{

    filters_path *new = build_filters_path();

    if (!new)
    {
        return NULL;
    }

    new->filter_params = ffmpeg_filter_builder(filters_descr, time_base, in_pix_fmt, out_pix_fmts, in_width, in_height, sample_aspect_ratio);

    if (!new->filter_params)
    {
        return NULL;
    }

    new->init = init_ffmpeg_filter;
    new->filter_frame = apply_ffmpeg_filter;
    new->uninit = un_init_ffmpeg_filter;
    new->internal = NULL;
    new->next = NULL;
    new->multiple_output = 1;
    return new;
}
