#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "logger.h"

int PRINT_DEBUG_ON = 1;

void logging(int level, const char *fmt, ...)
{
    if(level == INFO)
    {
        return;
    }
    if (PRINT_DEBUG_ON == 1)
    {
        va_list args;
        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        va_end(args);
        fprintf(stderr, "\n");
    }
}