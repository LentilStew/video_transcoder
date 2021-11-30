#include "filter.h"

void init_path(filters_path *path)
{

    if (path == NULL)
    {
        return;
    }

    path->init(path);
    init_path(path->next);
}

void uninit_path(filters_path *path)
{
    if (path == NULL)
    {
        return;
    }

    path->uninit(path);
    uninit_path(path->next);
}

AVFrame *apply_path(filters_path *path, AVFrame *frame)
{
    if (path == NULL)
    {
        return frame;
    }

    while (1)
    {
        frame = path->filter_frame(path, frame);

        if (frame == NULL)
        {
            break;
        }

        apply_path(path->next, frame);

        if (path->multiple_output == 0)
        {
            break;
        }
    }
}
void free_filter_path(filters_path *f)
{
    if (f == NULL)
    {
        return;
    }
    free_filter_path(f->next);
    free(f);
}