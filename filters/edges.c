#include "edges.h"
#include "filter.h"

#define IPOW2(n) n *n

uint8_t SQRTS2[IPOW2(255)];

#define CHARSQRT(n) (n >= 0xFE00) ? 0xFF : SQRTS2[n]

void init_edges(filters_path *filter_step)
{
    for (int i = 0; i < IPOW2(255); i++)
        SQRTS2[i] = round(sqrt(i));

    edges_params *params = (edges_params *)filter_step->filter_params;


}

void uninit_edges(filters_path *filter_step)
{
    return;
}

AVFrame *apply_edges(filters_path *a, AVFrame *frame)
{
    int h = frame->height;
    int src_linesize = frame->linesize[0];

    edges_params *params = (edges_params *)a->filter_params;

	AVFrame* new_frame = av_frame_alloc();

	new_frame->width = frame->width;
	new_frame->height = frame->height;
	new_frame->format = frame->format;
	new_frame->pts = frame->pts;

	av_frame_get_buffer(new_frame, 0);

    uint8_t* src = frame->data[0];
    
    uint8_t* dst = new_frame->data[0];

    int Gx, Gy;
    src += src_linesize;                                //skip first line of pixels
    src += 3;                                           //skip the first pixel from the second line
    for (int i = 0; i < h - 2; i++, dst += 6, src += 6) // skip first and last pixel of every line
    {

        for (int ii = 0; ii < src_linesize - 6; ii++, src++, dst++) //change pixel / panel
        {
            Gx = (-src[-src_linesize - 3] - src[-src_linesize] * 2 - src[-src_linesize + 3] +
                  src[src_linesize - 3] + src[src_linesize] * 2 + src[src_linesize + 3]);

            Gy = (-src[-src_linesize - 3] - src[-3] * 2 - src[src_linesize - 3] +
                  src[-src_linesize + 3] + src[3] * 2 + src[src_linesize + 3]);

            dst[0] = CHARSQRT(IPOW2(Gx) + IPOW2(Gy));
        }
    }

    av_frame_free(&frame);

    return new_frame;
}

edges_params *edges_builder(int height, int width)
{
    edges_params *params = malloc(sizeof(edges_params));
    if (!params)
    {
        return NULL;
    }
    params->height = height;
    params->width = width;
    return params;
}

filters_path *build_edges_filter(int width, int height)
{
    filters_path *new = build_filters_path();
    if (!new)
    {
        return NULL;
    }
    new->filter_params = edges_builder(height, width);
    if (!new->filter_params)
    {
        return NULL;
    }
    new->init = init_edges;
    new->filter_frame = apply_edges;
    new->uninit = uninit_edges;
    new->internal = NULL;
    new->next = NULL;
    new->multiple_output = 0;
    return new;
}
