#ifndef __DEFINE_H__
#define __DEFINE_H__

//#include "legup/streaming.h"

#define WIDTH 128
#define HEIGHT 128

typedef struct {
    int startrow;
    int endrow;
    int result;
} thread_arg;

// add func
void *add(void *threadarg);

#endif
