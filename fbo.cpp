#include "fbo.h"
#include "image.h"
#include "queue.h"
#include "lib/GLAD/glad.h"
#include "lib/util/vec.h"
#include "lib/util/io.h"

struct framebuffer {
    GLuint fbo, rbo, tex;
};

static GLuint activefbo = 0;
static Image activefboimg = LIBDRAW_CONST(SCREEN);
static vector<framebuffer> fbos;

GLuint createfbo(Image image) {
    ImageMeta& meta = findimg(image);

    GLuint fbo = 0, rbo = 0;
    glGenFramebuffers(1, &fbo);
    glGenRenderbuffers(1, &rbo);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, meta.id, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, meta.w, meta.h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);
    if (GLenum err = glGetError()) println("Failed to create renderbuffer: ", (int)err);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)   
        println("Framebuffer is incomplete: ", (int)glCheckFramebufferStatus(GL_FRAMEBUFFER));

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    fbos.push({ fbo, rbo, meta.id });

    return fbo;
}

void bindfbo(Image image) {
    activefboimg = image;
    ImageMeta* meta = &findimg(image);
    Image id = image;
    while (meta->parent > 0) id = meta->parent, meta = &findimg(meta->parent);
    GLuint fbo = 0;
    if (meta->parent < 0) fbo = -meta->parent;
    else fbo = -(meta->parent = -(int)createfbo({ id }));

    if (activefbo != fbo) {
        activefbo = fbo;
        // if (UNIFORM_WIDTH == -1) UNIFORM_WIDTH = glGetUniformLocation(shader, "width");
        // if (UNIFORM_HEIGHT == -1) UNIFORM_HEIGHT = glGetUniformLocation(shader, "height");
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        int width = meta->w, height = meta->h;
        // glUniform1i(UNIFORM_WIDTH, images[image.id].width);
        // glUniform1i(UNIFORM_HEIGHT, images[image.id].height);
        glViewport(0, 0, width, height);
        glClearColor(0, 0, 0, 0);
        ensure3d();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); 
        // bindtexture({ 0 });
    }
}

Image currentfbo() {
    return activefboimg;
}

void unbindfbo() {
    if (activefbo != 0) {
        activefbo = 0;
        // if (UNIFORM_WIDTH == -1) UNIFORM_WIDTH = glGetUniformLocation(shader, "width");
        // if (UNIFORM_HEIGHT == -1) UNIFORM_HEIGHT = glGetUniformLocation(shader, "height");
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        // glUniform1i(find_uniform("width"), findimg(0).w);
        // glUniform1i(find_uniform("height"), findimg(0).h);
        glViewport(0, 0, findimg(1).w, findimg(1).h); // image 1 is default fbo
        glClearColor(0, 0, 0, 1);
        ensure3d();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); 
    }
}

ImageMeta init_default_fbo(int width, int height) {
    GLuint fbtex, fbo, rbo;
    glGenTextures(1, &fbtex);
    glGenFramebuffers(1, &fbo);
    glGenRenderbuffers(1, &rbo);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glBindTexture(GL_TEXTURE_2D, fbtex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fbtex, 0);

    if (GLenum err = glGetError()) println("Failed to create default framebuffer texture: ", (int)err);

    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);

    if (GLenum err = glGetError()) println("Failed to create default renderbuffer: ", (int)err);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    fbos.push({ fbo, rbo, fbtex });
    return { 0, 0, width, height, fbtex, { -int(fbo) } };
}