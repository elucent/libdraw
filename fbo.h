#ifndef _LIBDRAW_FBO_H
#define _LIBDRAW_FBO_H

#include "draw.h"
#include "image.h"

GLuint createfbo(Image img);
void bindfbo(Image img);
void unbindfbo();
ImageMeta init_default_fbo(int width, int height);
Image currentfbo();

#endif