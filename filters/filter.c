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

void apply_path(filters_path *path, AVFrame *frame)
{
    if (path->next == NULL)
    {
        return;
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

filters_path * append_filter_path(filters_path *a, filters_path *b)
{
    filters_path *root = a;
    if(a == NULL)
    {
        return NULL;
    }

    while(a->next !=  NULL)
    {
        a = a->next;
    }
    
    a->next = b;
    
    return root;
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

filters_path *build_filters_path()
{
    filters_path *new = malloc(sizeof(filters_path));
    if (new == NULL)
    {
        return NULL;
    }

    new->encoder_filter_path = NULL;
    new->filter_frame = NULL;
    new->filter_params = NULL;
    new->init = NULL;
    new->internal = NULL;
    new->multiple_output = 0;
    new->next = NULL;
    new->uninit = NULL;
    return new;
}