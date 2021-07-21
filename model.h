#ifndef _LIBDRAW_MODEL_H
#define _LIBDRAW_MODEL_H

#include "draw.h"
#include "lib/util/vec.h"
#include "lib/GLAD/glad.h"

struct Buffer {
    vector<float> verts, cols, norms, uvs, sprs;
    GLuint vbuf, cbuf, nbuf, tbuf, sbuf;
    bool dirty;

    Buffer();
    void bake();
    bool empty() const;
    void reset();
    void draw();
    void takefrom(const Buffer& buf, float dx, float dy, float dz, float r, float g, float b, float a);

    void pos(float x, float y, float z);
    void col(float r, float g, float b, float a);
    void uv(float u, float v);
    void spr(float x, float y, float w, float h);
    void norm(float x, float y, float z);
};

Buffer& findbuf(Model model);
Model init_render_buffer();
Model create_new_model();

#endif