#ifndef _LIBDRAW_IMAGE_H
#define _LIBDRAW_IMAGE_H

#include "draw.h"
#include "lib/GLAD/glad.h"

struct ImageMeta {
    int x, y, w, h;
    GLuint id;
    Image parent;
};

ImageMeta& findimg(Image i);
void init_images(int width, int height);

#endif