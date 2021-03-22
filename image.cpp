#include "image.h"
#include "lib/SOIL/SOIL.h"
#include "lib/util/vec.h"
#include "fbo.h"

static vector<ImageMeta> images;

ImageMeta& findimg(Image img) {
    return images[img];
}

static Image createimg(ImageMeta meta) {
    images.push(meta);
    return images.size() - 1;
}

extern "C" Image LIBDRAW_SYMBOL(image)(const char* path) {
    int width, height, channels;
    unsigned char* img = SOIL_load_image(path, &width, &height, &channels, SOIL_LOAD_RGBA);

    GLuint id;
    glGenTextures(1, &id);
        
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, 
                    GL_UNSIGNED_BYTE, img);
    glBindTexture(GL_TEXTURE_2D, 0);

    SOIL_free_image_data(img);

    Image result = createimg({ 0, 0, width, height, id });
    // printf("loaded image %s into id %d\n", path, result.id);
    return result;
}

void init_images(int width, int height) {
    GLuint* data = new GLuint[8 * 8];
    for (int i = 0; i < 64; i ++) data[i] = 0xffffffff;

    GLuint id;
    glGenTextures(1, &id);
        
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 8, 8, 0, GL_RGBA, 
                    GL_UNSIGNED_BYTE, data);
    glBindTexture(GL_TEXTURE_2D, 0);

    delete[] data;
    images.push({ 0, 0, 8, 8, id, { 0 }});
    images.push(init_default_fbo(width, height));
    LIBDRAW_CONST(BLANK) = 0;
    LIBDRAW_CONST(SCREEN) = 1;
}

Image LIBDRAW_CONST(SCREEN);
Image LIBDRAW_CONST(BLANK);

extern "C" Image LIBDRAW_SYMBOL(newimage)(int w, int h) {
    GLuint* data = new GLuint[w * h];
    for (int i = 0; i < w * h; i ++) data[i] = 0xffffffff;

    GLuint id;
    glGenTextures(1, &id);
        
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, 
                    GL_UNSIGNED_BYTE, data);
    glBindTexture(GL_TEXTURE_2D, 0);

    delete[] data;
    images.push({ 0, 0, w, h, id, { 0 }});
    return images.size() - 1;
}

extern "C" Image LIBDRAW_SYMBOL(subimage)(Image i, int x, int y, int w, int h) {
    ImageMeta& meta = findimg(i);
    images.push({ x, y, w, h, meta.id, i });
    return images.size() - 1;
}

extern "C" int LIBDRAW_SYMBOL(width)(Image i) {
    return findimg(i).w;
}

extern "C" int LIBDRAW_SYMBOL(height)(Image i) {
    return findimg(i).h;
}