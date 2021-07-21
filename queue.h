#ifndef _LIBDRAW_QUEUE_H
#define _LIBDRAW_QUEUE_H

#include "draw.h"

extern float projection[4][4], view[4][4], transform[4][4];
extern bool invert;

enum StepType {
    STEP_SET_COLOR,
    STEP_SET_ORIGIN,
    STEP_RECT,
    STEP_POLYGON,
    STEP_SPRITE,
    STEP_FONT,
    STEP_TEXT,
    STEP_WRAPPED_TEXT,
    STEP_CUBE, 
    STEP_BOARD,
    STEP_SLANT,
    STEP_PRISM,
    STEP_CONE,
    STEP_HEDRON,
    STEP_ORTHO,
    STEP_FRUSTUM,
    STEP_PAN,
    STEP_TILT,
    STEP_LOOK,
    STEP_ROTATE,
    STEP_SCALE,
    STEP_TRANSLATE,
    STEP_RENDER,
    STEP_BEGIN,
    STEP_FOG,
    STEP_END
};

struct Step {
    StepType type;
    union {
        struct { Color color; } set_color;
        struct { Origin origin; } set_origin;
        struct { float x, y, w, h; } rect;
        struct { float x, y, r; int n; } polygon;
        struct { float x, y, w, h; Image img; } sprite;
        struct { Image img; } font;
        struct { float x, y; const char* str; } text;
        struct { float x, y; const char* str; float width; } wraptext;
        struct { float x, y, z, w, h, l; Texture tex; } cube;
        struct { float x, y, z, w, h, l; Edge edge; Texture tex; } slant;
        struct { float x, y, z, w, h, l; int n; Axis axis; Texture tex; } prism;
        struct { float x, y, z, w, h, l; int n; Direction dir; Texture tex; } cone;
        struct { float x, y, z, w, h, l; int m, n; Texture tex; } hedron;
        struct { float x, y, z, w, h; Image img; } board;
        struct { float w, h; } ortho;
        struct { float w, h, fov; } frustum;
        struct { float x, y, z; } pan;
        struct { float degrees; Axis axis; } tilt;
        struct { float x, y, z, yaw, pitch; } look;
        struct { float angle; Axis axis; } rotate;
        struct { float x, y, z; } scale;
        struct { float x, y, z; } translate;
        struct { Model model; Image img; } render;
        struct { Color color; float range; } fog;
    } data;
};

void identity(float matrix[4][4]);
void enqueue(const Step& step);
void flush(Model model);
void stretched_sprite(float x, float y, float w, float h, Image img);
void ensure2d();
void ensure3d();
void init_queue();
Model getrendermodel();
void apply_default_uniforms();

#endif