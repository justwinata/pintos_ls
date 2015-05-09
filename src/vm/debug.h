#include <stdio.h>
#include <stdlib.h>

#ifndef DEBUG_H
#define DEBUG_H

#ifdef NDEBUG
#define DEBUG(m, ...)
#else
#define DEBUG(m, ...)   fprintf(stderr, "[DEBUG] %s:%d: " m "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#endif
