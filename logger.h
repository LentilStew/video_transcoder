#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#define ERROR 0
#define INFO 1

void logging(int level, const char *fmt, ...);