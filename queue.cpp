#include "queue.h"
#include "model.h"
#include "string.h"
#include "math.h"
#include "image.h"
#include "shader.h"
#include "fbo.h"
#include "lib/util/io.h"
#include "lib/util/vec.h"

static GLuint texture;
static Origin orig;
float projection[4][4], view[4][4], transform[4][4];
static float pi = 3.14159265358979323f;
static float red = 1, green = 1, blue = 1, alpha = 1;
static float lightx = 0.218218, lighty = -0.872872, lightz = 0.436436;
static bool mode3d = false;
static Model rendermodel;
bool invert = false;
static float yaw = 0, pitch = 0, roll = 0;
static float camerax = 0, cameray = 0, cameraz = 0;
static Image currentfont;

struct mat4 {
    float data[4][4];
};

static float boardh[3], boardv[3];
static vector<mat4> matstack;

void identity(float matrix[4][4]) {
    for (int i = 0; i < 4; i ++) {
        for (int j = 0; j < 4; j ++) {
            if (i == j) matrix[i][j] = 1;
            else matrix[i][j] = 0;
        }
    }
}

static void matmult(float a[4][4], float b[4][4]) {
    float t[4][4];
    memcpy(t, a, sizeof(float) * 4 * 4);
    for (int i = 0; i < 4; i ++) {
        for (int j = 0; j < 4; j ++) {
            a[i][j] = 0;
            for (int k = 0; k < 4; k ++) {
                a[i][j] += t[i][k] * b[k][j];
            }
        }
    }
}

static void matset(float a[4][4], float b[4][4]) {
    memcpy(a, b, sizeof(float) * 4 * 4);
}

static void rotatex(float matrix[4][4], float degrees) {
    float a = degrees * pi / 180;
    float rotmatrix[4][4] = {
        { 1, 0, 0, 0 },
        { 0, cos(a), -sin(a), 0 },
        { 0, sin(a), cos(a), 0 },
        { 0, 0, 0, 1 }
    };
    matmult(matrix, rotmatrix);
}

static void rotatey(float matrix[4][4], float degrees) {
    float a = degrees * pi / 180;
    float rotmatrix[4][4] = {
        { cos(a), 0, -sin(a), 0 },
        { 0, 1, 0, 0 },
        { sin(a), 0, cos(a), 0 },
        { 0, 0, 0, 1 }
    };
    matmult(matrix, rotmatrix);
}

static void rotatez(float matrix[4][4], float degrees) {
    float a = -degrees * pi / 180;
    float rotmatrix[4][4] = {
        { cos(a), -sin(a), 0, 0 },
        { sin(a), cos(a), 0, 0 },
        { 0, 0, 1, 0 },
        { 0, 0, 0, 1 }
    };
    matmult(matrix, rotmatrix);
}

static void scale(float matrix[4][4], float x, float y, float z) {
    float scalematrix[4][4] = {
        { x, 0, 0, 0 },
        { 0, y, 0, 0 },
        { 0, 0, z, 0 },
        { 0, 0, 0, 1 }
    };
    matmult(matrix, scalematrix);
}

static void translate(float matrix[4][4], float x, float y, float z) {
    float translation[4][4] = {
        { 1, 0, 0, 0 },
        { 0, 1, 0, 0 },
        { 0, 0, 1, 0 },
        { x, y, z, 1 }
    };
    matmult(matrix, translation);
}

static void ortho(float matrix[4][4], float w, float h) {
    float near = -1000, far = 1000;
    float ortho[4][4] = {
        { 2 / w, 0, 0, 0 },
        { 0, -2 / h, 0, 0 },
        { 0, 0, -2 / (far - near), 0 },
        { -1, 1, -(far + near) / (far - near), 1 }
    };
    matmult(matrix, ortho);
}

static void frustum(float matrix[4][4], float w, float h, float fov) {
    float aspect = w / h, near = 0.125, far = 1024, 
        top = near * tan(fov * pi / 180), bottom = -top, right = aspect * top, left = -right;
    float frustum[4][4] = {
        { 2 * near / (right - left), 0, 0, 0 },
        { 0, 2 * near / (top - bottom), 0, 0 },
        { 0, 0, -(far + near) / (far - near), -1 },
        { -near * (right + left) / (right - left), -near * (top + bottom) / (top - bottom), 2 * far * near / (near - far), 0 }
    };
    // float frustum[4][4] = {
    //     { 2 * near / (right - left), 0, 0, -near * (right + left) / (right - left) },
    //     { 0, 2 * near / (top - bottom), 0, -near * (top + bottom) / (top - bottom) },
    //     { 0, 0, -(far + near) / (far - near), 2 * far * near / (near - far) },
    //     { 0, 0, -1, 0 }
    // };
    matmult(matrix, frustum);
}

static vector<Step> steps;

void enqueue(const Step& step) {
    steps.push(step);
}

static void drawbuf(Buffer& buf) {
    glUniform1i(find_uniform("inverted"), invert ? 1 : 0);
    if (!buf.empty()) {
        buf.draw();
    }
    glUniform1i(find_uniform("inverted"), 0);
}

static bool stateful(const Step& step) {
    switch (step.type) {
        case STEP_SET_COLOR:
        case STEP_SET_ORIGIN:
        case STEP_BEGIN:
        case STEP_FONT:
            return false;
        case STEP_RECT:
        case STEP_POLYGON:
            return mode3d || texture != findimg(BLANK).id;
        case STEP_SPRITE:
            return mode3d || findimg(step.data.sprite.img).id != texture;
        case STEP_TEXT:
        case STEP_WRAPPED_TEXT:
            return mode3d || findimg(currentfont).id != texture;
        case STEP_BOARD:
            return !mode3d || findimg(step.data.board.img).id != texture;
        case STEP_CUBE:
            return !mode3d || findimg(step.data.cube.tex.img).id != texture;
        case STEP_SLANT:
            return !mode3d || findimg(step.data.slant.tex.img).id != texture;
        case STEP_PRISM:
            return !mode3d || findimg(step.data.prism.tex.img).id != texture;
        case STEP_CONE:
            return !mode3d || findimg(step.data.cone.tex.img).id != texture;
        case STEP_HEDRON:
            return !mode3d || findimg(step.data.hedron.tex.img).id != texture;
        case STEP_ORTHO:
        case STEP_FRUSTUM:
        case STEP_PAN:
        case STEP_TILT:
        case STEP_LOOK:
        case STEP_ROTATE:
        case STEP_SCALE:
        case STEP_TRANSLATE:
        case STEP_RENDER:
        case STEP_END:
        case STEP_FOG:
            return true;
        default:
            return false;
    }
}

static void bindtex(Image i) {
    if (texture != findimg(i).id) {
        glBindTexture(GL_TEXTURE_2D, findimg(i).id);
        texture = findimg(i).id;
    }
}

static void plane(Buffer& buf, 
    float x, float y, float z, float w, float h,  
    float hx, float hy, float hz, 
    float vx, float vy, float vz,
    Image i) {
    bindtex(i);
    ImageMeta* meta = &findimg(i);
    while (meta->parent > 0) meta = &findimg(meta->parent);

    float nx = -(hy * vz - hz * vy), ny = -(hz * vx - hx * vz), nz = -(hx * vy - hy * vx);
    float u = float(findimg(i).x) / meta->w, v = float(findimg(i).y) / meta->h;
    float uw = float(findimg(i).w) / meta->w, vh = float(findimg(i).h) / meta->h;

    if (w < 0) w *= -1, u += uw, uw *= -1;
    if (h < 0) h *= -1, v += vh, vh *= -1;

    float ox = int(orig) % 3, oy = int(orig) % 9 / 3;
    x -= 0.5f * ox * w * hx + 0.5f * oy * h * vx; 
    y -= 0.5f * ox * w * hy + 0.5f * oy * h * vy; 
    z -= 0.5f * ox * w * hz + 0.5f * oy * h * vz;
    buf.pos(x + w * hx, y + w * hy, z + w * hz); 
    buf.pos(x, y, z); 
    buf.pos(x + h * vx, y + h * vy, z + h * vz);
    buf.pos(x + h * vx, y + h * vy, z + h * vz);
    buf.pos(x + w * hx + h * vx, y + w * hy + h * vy, z + w * hz + h * vz); 
    buf.pos(x + w * hx, y + w * hy, z + w * hz); 
    for (int i = 0; i < 6; i ++) buf.col(red, green, blue, alpha);
    for (int i = 0; i < 6; i ++) buf.norm(nx, ny, nz);
    buf.uv(1, 0); buf.uv(0, 0); buf.uv(0, 1);
    buf.uv(0, 1); buf.uv(1, 1); buf.uv(1, 0);
    for (int i = 0; i < 6; i ++) buf.spr(u, v, uw, vh);
}

static void scaleduv(Buffer& buf, float u, float v, float w, float h, float iw, float ih) {
    buf.uv((u + w) / iw, (v + h) / ih);
    buf.uv((u) / iw, (v + h) / ih);
    buf.uv((u) / iw, (v) / ih);
    buf.uv((u) / iw, (v) / ih);
    buf.uv((u + w) / iw, (v) / ih);
    buf.uv((u + w) / iw, (v + h) / ih);
}

static void scaleduvx(Buffer& buf, float u, float v, float w, float h, float iw, float ih) {
    buf.uv((u + w) / iw, (v + h) / ih);
    buf.uv((u + w) / iw, (v) / ih);
    buf.uv((u) / iw, (v) / ih);
    buf.uv((u) / iw, (v) / ih);
    buf.uv((u) / iw, (v + h) / ih);
    buf.uv((u + w) / iw, (v + h) / ih);
}

static float frem(float a, float b) {
    float denom = floor(a / b);
    return a - denom * b;   
}

static void autouv(Buffer& buf, float u, float v, float w, float h, float iw, float ih) {
    float u_auto = frem(u, iw) * 2 / iw, v_auto = frem(v, ih) * 2 / ih, w_auto = w / iw, h_auto = h / ih;
    buf.uv(u_auto + w_auto, v_auto + h_auto);
    buf.uv(u_auto, v_auto + h_auto);
    buf.uv(u_auto, v_auto);
    buf.uv(u_auto, v_auto);
    buf.uv(u_auto + w_auto, v_auto);
    buf.uv(u_auto + w_auto, v_auto + h_auto);
}

static void stretchuv(Buffer& buf) {
    buf.uv(1, 1);
    buf.uv(0, 1);
    buf.uv(0, 0);
    buf.uv(0, 0);
    buf.uv(1, 0);
    buf.uv(1, 1);
}

static void polygon(Buffer& buf, float x, float y, float r, float n) {
    bindtex(BLANK);
    float ox = int(orig) % 3 - 1, oy = int(orig) % 9 / 3 - 1;
    x -= ox * r, y -= oy * r;
    float dt = -0.5 * pi;
    for (int i = 0; i < n; i ++) {
        float a1 = i * 2 * pi / n + dt, a2 = (i + 1) * 2 * pi / n + dt;
        buf.pos(x, y, 0);
        buf.pos(x + r * cos(a2), y + r * sin(a2), 0);
        buf.pos(x + r * cos(a1), y + r * sin(a1), 0);
        for (int i = 0; i < 3; i ++) buf.col(red, green, blue, alpha), buf.spr(0, 0, 1, 1), buf.norm(0, 0, -1);
        buf.uv(0.5, 0.5);
        buf.uv(0.5 + 0.5 * cos(a2), 0.5 + 0.5 * sin(a2));
        buf.uv(0.5 + 0.5 * cos(a1), 0.5 + 0.5 * sin(a1));
    }
}

static void cube(Buffer& buf, float x, float y, float z, float w, float h, float l, Texture tex) {
    Image i = tex.img;
    bindtex(i);
    ImageMeta* meta = &findimg(i);
    while (meta->parent > 0) meta = &findimg(meta->parent);
    float dx = w / 2, dy = h / 2, dz = l / 2;
    float u = float(findimg(i).x) / meta->w, v = float(findimg(i).y) / meta->h;
    float iw = float(findimg(i).w), ih = float(findimg(i).h);
    float uw = float(findimg(i).w) / meta->w, vh = float(findimg(i).h) / meta->h;
    float ox = int(orig) % 3 - 1, oy = int(orig) % 9 / 3 - 1, oz = int(orig) / 9 - 1;
    x -= w * ox / 2; y -= h * oy / 2; z -= l * oz / 2;

    // negative x
    buf.pos(x - dx, y - dy, z - dz); buf.pos(x - dx, y - dy, z + dz); buf.pos(x - dx, y + dy, z + dz);
    buf.pos(x - dx, y + dy, z + dz); buf.pos(x - dx, y + dy, z - dz); buf.pos(x - dx, y - dy, z - dz);

    // positive x
    buf.pos(x + dx, y - dy, z + dz); buf.pos(x + dx, y - dy, z - dz); buf.pos(x + dx, y + dy, z - dz);
    buf.pos(x + dx, y + dy, z - dz); buf.pos(x + dx, y + dy, z + dz); buf.pos(x + dx, y - dy, z + dz);

    // negative y
    buf.pos(x - dx, y - dy, z + dz); buf.pos(x - dx, y - dy, z - dz); buf.pos(x + dx, y - dy, z - dz);
    buf.pos(x + dx, y - dy, z - dz); buf.pos(x + dx, y - dy, z + dz); buf.pos(x - dx, y - dy, z + dz);

    // positive y
    buf.pos(x - dx, y + dy, z - dz); buf.pos(x - dx, y + dy, z + dz); buf.pos(x + dx, y + dy, z + dz);
    buf.pos(x + dx, y + dy, z + dz); buf.pos(x + dx, y + dy, z - dz); buf.pos(x - dx, y + dy, z - dz);

    // negative z
    buf.pos(x + dx, y - dy, z - dz); buf.pos(x - dx, y - dy, z - dz); buf.pos(x - dx, y + dy, z - dz);
    buf.pos(x - dx, y + dy, z - dz); buf.pos(x + dx, y + dy, z - dz); buf.pos(x + dx, y - dy, z - dz);

    // positive z
    buf.pos(x - dx, y - dy, z + dz); buf.pos(x + dx, y - dy, z + dz); buf.pos(x + dx, y + dy, z + dz);
    buf.pos(x + dx, y + dy, z + dz); buf.pos(x - dx, y + dy, z + dz); buf.pos(x - dx, y - dy, z + dz);

    for (int i = 0; i < 36; i ++) buf.col(red, green, blue, alpha);
    for (int i = 0; i < 6; i ++) buf.norm(-1, 0, 0);
    for (int i = 0; i < 6; i ++) buf.norm(1, 0, 0);
    for (int i = 0; i < 6; i ++) buf.norm(0, -1, 0);
    for (int i = 0; i < 6; i ++) buf.norm(0, 1, 0);
    for (int i = 0; i < 6; i ++) buf.norm(0, 0, -1);
    for (int i = 0; i < 6; i ++) buf.norm(0, 0, 1);

    switch (tex.type) {
        default:
        case LIBDRAW_CONST(TEX_NET):
            scaleduvx(buf, h, h, -h, w, iw, ih); // negative x
            scaleduvx(buf, h + l, h + w, h, -w, iw, ih); // positive x
            scaleduv(buf, h * 2 + l, h, w, l, iw, ih); // negative y
            scaleduv(buf, h, h, w, l, iw, ih); // positive y
            scaleduv(buf, h + l, h, -l, -h, iw, ih); // negative z
            scaleduv(buf, h, h + l, l, h, iw, ih); // positive z
            break;
        case LIBDRAW_CONST(TEX_AUTO):
            autouv(buf, z, y, l, h, iw, ih);
            autouv(buf, z, y, l, h, iw, ih);
            autouv(buf, z, x, l, w, iw, ih);
            autouv(buf, z, x, l, w, iw, ih);
            autouv(buf, x, y, w, h, iw, ih);
            autouv(buf, x, y, w, h, iw, ih);
            break;
        case LIBDRAW_CONST(TEX_STRETCH):
            for (int i = 0; i < 6; i ++) stretchuv(buf);
            break;
    }
    
    for (int i = 0; i < 36; i ++) buf.spr(u, v, uw, vh);
}

static void prism(Buffer& buf, float x, float y, float z, float w, float h, float l, int n, Axis axis, Texture tex) {
    Image i = tex.img;
    bindtex(i);
    ImageMeta* meta = &findimg(i);
    while (meta->parent > 0) meta = &findimg(meta->parent);
    float dx = w / 2, dy = h / 2, dz = l / 2;
    float u = float(findimg(i).x) / meta->w, v = float(findimg(i).y) / meta->h;
    float iw = float(findimg(i).w), ih = float(findimg(i).h);
    float uw = float(findimg(i).w) / meta->w, vh = float(findimg(i).h) / meta->h;
    float ox = int(orig) % 3 - 1, oy = int(orig) % 9 / 3 - 1, oz = int(orig) / 9 - 1;
    x -= w * ox / 2; y -= h * oy / 2; z -= l * oz / 2;

    float lv[3] = { 0, 0, 0 }, hv[3] = { 0, 0, 0 }, vv[3] = { 0, 0, 0 };
    float ln[3] = { 0, 0, 0 }, hn[3] = { 0, 0, 0 }, vn[3] = { 0, 0, 0 };
    float endw = 0, endh = 0;
    float sidew = 0, sideh = 0;
    switch (axis) {
        case X_AXIS:
            lv[0] = -dx, hv[2] = dz, vv[1] = dy;
            ln[0] = -1, hn[2] = 1, vn[1] = 1;
            endw = l, endh = h;
            sidew = 3 * l, sideh = w;
            break;
        case Y_AXIS:
            lv[1] = dy, hv[2] = dz, vv[0] = dx;
            ln[1] = 1, hn[2] = 1, vn[0] = 1;
            endw = w, endh = l;
            sidew = 3 * w, sideh = h;
            break;
        case Z_AXIS:
            lv[2] = dz, hv[0] = dx, vv[1] = dy;
            ln[2] = 1, hn[0] = 1, vn[1] = 1;
            endw = w, endh = h;
            sidew = 3 * w, sideh = l;
            break;
    }

    float frontx = x + lv[0], fronty = y + lv[1], frontz = z + lv[2];
    float backx = x - lv[0], backy = y - lv[1], backz = z - lv[2];

    // front
    for (int i = 0; i < n; i ++) {
        float a1 = 2 * pi * i / n - 0.5 * pi, a2 = 2 * pi * (i + 1) / n - 0.5 * pi;
        for (int j = 0; j < 3; j ++) {
            buf.norm(ln[0], ln[1], ln[2]); 
            buf.col(red, green, blue, alpha); 
            buf.spr(u, v, uw * (endw / iw), vh * (endh / ih));
        }
        float sa1 = sin(a1), ca1 = cos(a1), sa2 = sin(a2), ca2 = cos(a2);
        buf.pos(frontx, fronty, frontz);
        buf.pos(frontx + ca1 * hv[0] + sa1 * vv[0], fronty + ca1 * hv[1] + sa1 * vv[1], frontz + ca1 * hv[2] + sa1 * vv[2]);
        buf.pos(frontx + ca2 * hv[0] + sa2 * vv[0], fronty + ca2 * hv[1] + sa2 * vv[1], frontz + ca2 * hv[2] + sa2 * vv[2]);
        buf.uv(0.5, 0.5);
        buf.uv(0.5 + ca1 * 0.5, 0.5 + sa1 * 0.5);
        buf.uv(0.5 + ca2 * 0.5, 0.5 + sa2 * 0.5);
    }

    // back
    for (int i = 0; i < n; i ++) {
        float a1 = 2 * pi * i / n - 0.5 * pi, a2 = 2 * pi * (i + 1) / n - 0.5 * pi;
        for (int j = 0; j < 3; j ++) {
            buf.norm(-ln[0], -ln[1], -ln[2]); 
            buf.col(red, green, blue, alpha); 
            buf.spr(u + (2 * sidew / 3 / iw) * uw, v + (endh / ih + sideh / ih) * vh, uw * (endw / iw), vh * (endh / ih));
        }
        float sa1 = sin(a1), ca1 = cos(a1), sa2 = sin(a2), ca2 = cos(a2);
        buf.pos(backx, backy, backz);
        buf.pos(backx + ca2 * hv[0] + sa2 * vv[0], backy + ca2 * hv[1] + sa2 * vv[1], backz + ca2 * hv[2] + sa2 * vv[2]);
        buf.pos(backx + ca1 * hv[0] + sa1 * vv[0], backy + ca1 * hv[1] + sa1 * vv[1], backz + ca1 * hv[2] + sa1 * vv[2]);
        buf.uv(0.5, 0.5);
        buf.uv(0.5 + ca2 * 0.5, 0.5 + sa2 * 0.5);
        buf.uv(0.5 + ca1 * 0.5, 0.5 + sa1 * 0.5);
    }

    // sides
    for (int i = 0; i < n; i ++) {
        float a1 = 2 * pi * i / n - 0.5 * pi, a2 = 2 * pi * (i + 1) / n - 0.5 * pi;
        float sa1 = sin(a1), ca1 = cos(a1), sa2 = sin(a2), ca2 = cos(a2);
        for (int j = 0; j < 6; j ++) {
            buf.norm(
                0.5 * ca1 * hn[0] + ca2 * hn[0] + 0.5 * sa1 * vn[0] + sa2 * vn[0],
                0.5 * ca1 * hn[1] + ca2 * hn[1] + 0.5 * sa1 * vn[1] + sa2 * vn[1],
                0.5 * ca1 * hn[2] + ca2 * hn[2] + 0.5 * sa1 * vn[2] + sa2 * vn[2]
            ); 
            buf.col(red, green, blue, alpha); 
            buf.spr(u, v + (endh / ih) * vh, uw * (sidew / iw), vh * (sideh / ih));
        }
        
        buf.pos(backx + ca1 * hv[0] + sa1 * vv[0], backy + ca1 * hv[1] + sa1 * vv[1], backz + ca1 * hv[2] + sa1 * vv[2]);
        buf.pos(backx + ca2 * hv[0] + sa2 * vv[0], backy + ca2 * hv[1] + sa2 * vv[1], backz + ca2 * hv[2] + sa2 * vv[2]);
        buf.pos(frontx + ca2 * hv[0] + sa2 * vv[0], fronty + ca2 * hv[1] + sa2 * vv[1], frontz + ca2 * hv[2] + sa2 * vv[2]);
        buf.pos(frontx + ca2 * hv[0] + sa2 * vv[0], fronty + ca2 * hv[1] + sa2 * vv[1], frontz + ca2 * hv[2] + sa2 * vv[2]);
        buf.pos(frontx + ca1 * hv[0] + sa1 * vv[0], fronty + ca1 * hv[1] + sa1 * vv[1], frontz + ca1 * hv[2] + sa1 * vv[2]);
        buf.pos(backx + ca1 * hv[0] + sa1 * vv[0], backy + ca1 * hv[1] + sa1 * vv[1], backz + ca1 * hv[2] + sa1 * vv[2]);

        buf.uv(float(i) / n, 0); buf.uv(float(i + 1) / n, 0); buf.uv(float(i + 1) / n, 1);
        buf.uv(float(i + 1) / n, 1); buf.uv(float(i) / n, 1); buf.uv(float(i) / n, 0);
    }
}

static void pyramid(Buffer& buf, float x, float y, float z, float w, float h, float l, int n, Direction dir, Texture tex) {
    Image i = tex.img;
    Axis axis = (Axis)(dir / 2);
    bindtex(i);
    ImageMeta* meta = &findimg(i);
    while (meta->parent > 0) meta = &findimg(meta->parent);
    float dx = w / 2, dy = h / 2, dz = l / 2;
    float u = float(findimg(i).x) / meta->w, v = float(findimg(i).y) / meta->h;
    float iw = float(findimg(i).w), ih = float(findimg(i).h);
    float uw = float(findimg(i).w) / meta->w, vh = float(findimg(i).h) / meta->h;
    float ox = int(orig) % 3 - 1, oy = int(orig) % 9 / 3 - 1, oz = int(orig) / 9 - 1;

    x -= w * ox / 2; y -= h * oy / 2; z -= l * oz / 2;

    bool flipped = dir % 2 == 0;
    if (axis == X_AXIS) flipped = !flipped;

    float lv[3] = { 0, 0, 0 }, hv[3] = { 0, 0, 0 }, vv[3] = { 0, 0, 0 };
    float ln[3] = { 0, 0, 0 }, hn[3] = { 0, 0, 0 }, vn[3] = { 0, 0, 0 };
    float endw = 0, endh = 0;
    float sidew = 0, sideh = 0;
    switch (axis) {
        case X_AXIS:
            lv[0] = -dx, hv[2] = dz, vv[1] = dy;
            ln[0] = -1, hn[2] = 1, vn[1] = 1;
            endw = l, endh = h;
            sidew = 3 * l, sideh = w;
            break;
        case Y_AXIS:
            lv[1] = dy, hv[2] = dz, vv[0] = dx;
            ln[1] = 1, hn[2] = 1, vn[0] = 1;
            endw = w, endh = l;
            sidew = 3 * w, sideh = h;
            break;
        case Z_AXIS:
            lv[2] = dz, hv[0] = dx, vv[1] = dy;
            ln[2] = 1, hn[0] = 1, vn[1] = 1;
            endw = w, endh = h;
            sidew = 3 * w, sideh = l;
            break;
    }

    float frontx = x + lv[0], fronty = y + lv[1], frontz = z + lv[2];
    float backx = x - lv[0], backy = y - lv[1], backz = z - lv[2];

    // front
    if (flipped) for (int i = 0; i < n; i ++) {
        float a1 = 2 * pi * i / n - 0.5 * pi, a2 = 2 * pi * (i + 1) / n - 0.5 * pi;
        for (int j = 0; j < 3; j ++) {
            buf.norm(ln[0], ln[1], ln[2]); 
            buf.col(red, green, blue, alpha); 
            buf.spr(u, v, uw * (endw / iw), vh * (endh / ih));
        }
        float sa1 = sin(a1), ca1 = cos(a1), sa2 = sin(a2), ca2 = cos(a2);
        buf.pos(frontx, fronty, frontz);
        buf.pos(frontx + ca1 * hv[0] + sa1 * vv[0], fronty + ca1 * hv[1] + sa1 * vv[1], frontz + ca1 * hv[2] + sa1 * vv[2]);
        buf.pos(frontx + ca2 * hv[0] + sa2 * vv[0], fronty + ca2 * hv[1] + sa2 * vv[1], frontz + ca2 * hv[2] + sa2 * vv[2]);
        buf.uv(0.5, 0.5);
        buf.uv(0.5 + ca1 * 0.5, 0.5 + sa1 * 0.5);
        buf.uv(0.5 + ca2 * 0.5, 0.5 + sa2 * 0.5);
    }

    // back
    if (!flipped) for (int i = 0; i < n; i ++) {
        float a1 = 2 * pi * i / n - 0.5 * pi, a2 = 2 * pi * (i + 1) / n - 0.5 * pi;
        for (int j = 0; j < 3; j ++) {
            buf.norm(-ln[0], -ln[1], -ln[2]); 
            buf.col(red, green, blue, alpha); 
            buf.spr(u, v, uw * (endw / iw), vh * (endh / ih));
        }
        float sa1 = sin(a1), ca1 = cos(a1), sa2 = sin(a2), ca2 = cos(a2);
        buf.pos(backx, backy, backz);
        buf.pos(backx + ca2 * hv[0] + sa2 * vv[0], backy + ca2 * hv[1] + sa2 * vv[1], backz + ca2 * hv[2] + sa2 * vv[2]);
        buf.pos(backx + ca1 * hv[0] + sa1 * vv[0], backy + ca1 * hv[1] + sa1 * vv[1], backz + ca1 * hv[2] + sa1 * vv[2]);
        buf.uv(0.5, 0.5);
        buf.uv(0.5 + ca2 * 0.5, 0.5 + sa2 * 0.5);
        buf.uv(0.5 + ca1 * 0.5, 0.5 + sa1 * 0.5);
    }

    // sides
    for (int i = 0; i < n; i ++) {
        float a1 = 2 * pi * i / n - 0.5 * pi, a2 = 2 * pi * (i + 1) / n - 0.5 * pi;
        float sa1 = sin(a1), ca1 = cos(a1), sa2 = sin(a2), ca2 = cos(a2);
        float rx1 = ca1 * hv[0] + sa1 * vv[0], ry1 = ca1 * hv[1] + sa1 * vv[1], rz1 = ca1 * hv[2] + sa1 * vv[2];
        float rx2 = ca2 * hv[0] + sa2 * vv[0], ry2 = ca2 * hv[1] + sa2 * vv[1], rz2 = ca2 * hv[2] + sa2 * vv[2];
        float rx = (rx1 + rx2) / 2, ry = (ry1 + ry2) / 2, rz = (rz1 + rz2) / 2;
        float r = sqrt(rx * rx + ry * ry + rz * rz), hl = sqrt(lv[0] * lv[0] + lv[1] * lv[1] + lv[2] * lv[2]);
        float yaw = (a1 + a2) / 2, pitch = atan2(r, hl);
        float nx = sin(yaw) * cos(pitch), ny = sin(pitch), nz = cos(yaw) * cos(pitch);
        for (int j = 0; j < 3; j ++) {
            buf.norm(nx, ny, nz); 
            buf.col(red, green, blue, alpha); 
            buf.spr(u, v + (endh / ih) * vh, uw * (sidew / iw), vh * (sideh / ih));
        }
        
        if (flipped) {
            buf.pos(backx, backy, backz);
            buf.pos(frontx + rx2, fronty + ry2, frontz + rz2);
            buf.pos(frontx + rx1, fronty + ry1, frontz + rz1);
        }
        else {
            buf.pos(backx + rx1, backy + ry1, backz + rz1);
            buf.pos(backx + rx2, backy + ry2, backz + rz2);
            buf.pos(frontx, fronty, frontz);
        }

        if (flipped) {
            buf.uv(float(i) / n, 0); buf.uv(float(i + 1) / n, 1); buf.uv(float(i + 0.5) / n, 1);
        }
        else {
            buf.uv(float(i) / n, 1); buf.uv(float(i + 1) / n, 1); buf.uv(float(i + 0.5) / n, 0);
        }
    }
}

static void hedron(Buffer& buf, float x, float y, float z, float w, float h, float l, int m, int n, Texture tex) {
    Image i = tex.img;
    bindtex(i);
    ImageMeta* meta = &findimg(i);
    while (meta->parent > 0) meta = &findimg(meta->parent);
    float dx = w / 2, dy = h / 2, dz = l / 2;
    float u = float(findimg(i).x) / meta->w, v = float(findimg(i).y) / meta->h;
    float iw = float(findimg(i).w), ih = float(findimg(i).h);
    float uw = float(findimg(i).w) / meta->w, vh = float(findimg(i).h) / meta->h;
    float ox = int(orig) % 3 - 1, oy = int(orig) % 9 / 3 - 1, oz = int(orig) / 9 - 1;
    x -= w * ox / 2; y -= h * oy / 2; z -= l * oz / 2;

    for (int i = 0; i < m; i ++) {
        for (int j = 0; j < n; j ++) {
            float y1 = 2 * pi * i / m - 0.5 * pi, y2 = 2 * pi * (i + 1) / m - 0.5 * pi;
            float p1 = pi * j / n - 0.5 * pi, p2 = pi * (j + 1) / n - 0.5 * pi;
            float sy1 = sin(y1), cy1 = cos(y1), sy2 = sin(y2), cy2 = cos(y2);
            float sp1 = sin(p1), cp1 = cos(p1), sp2 = sin(p2), cp2 = cos(p2);
            float nx = (cp1 * sy1 + cp1 * sy2 + cp2 * sy2 + cp2 * sy1) / 4,
                  ny = (sp1 + sp2) / 2,
                  nz = (cp1 * cy1 + cp1 * cy2 + cp2 * cy2 + cp2 * cy1) / 4;
            for (int k = 0; k < 6; k ++) {
                buf.norm(nx, ny, nz); 
                buf.col(red, green, blue, alpha); 
                buf.spr(u, v, uw * (3 * w / iw), vh * (h / ih));
            }
            buf.pos(x + dx * cp1 * sy1, y + dy * sp1, z + dz * cp1 * cy1);
            buf.pos(x + dx * cp1 * sy2, y + dy * sp1, z + dz * cp1 * cy2);
            buf.pos(x + dx * cp2 * sy2, y + dy * sp2, z + dz * cp2 * cy2);
            buf.pos(x + dx * cp2 * sy2, y + dy * sp2, z + dz * cp2 * cy2);
            buf.pos(x + dx * cp2 * sy1, y + dy * sp2, z + dz * cp2 * cy1);
            buf.pos(x + dx * cp1 * sy1, y + dy * sp1, z + dz * cp1 * cy1);
            buf.uv(float(i) / m, float(j) / n);
            buf.uv(float(i + 1) / m, float(j) / n);
            buf.uv(float(i + 1) / m, float(j + 1) / n);
            buf.uv(float(i + 1) / m, float(j + 1) / n);
            buf.uv(float(i) / m, float(j + 1) / n);
            buf.uv(float(i) / m, float(j) / n);
        }
    }
}

static void text(Buffer& buf, float x, float y, const char* str, float width) {
    bindtex(currentfont);
    int iw = ::width(currentfont), ih = ::height(currentfont);
    int cw = iw / 32, ch = ih / 32;
    float ox = x, oy = y;
    const char* reader = str;
    const char* prevspace = str, *nextspace = str;
    int length = strlen(str);
    ImageMeta* meta = &findimg(currentfont);
    while (meta->parent > 0) meta = &findimg(meta->parent);
    while (*reader && reader - str < length) {
        if (*reader == ' ' || *reader == '\t' || *reader == '\n') {
            prevspace = reader;
            nextspace = reader + 1;
            while (*nextspace && *nextspace != ' ' && *nextspace != '\t' && *nextspace != '\n')
                nextspace ++;
        }
        if (*reader == ' ') {
            x += cw;
            ++ reader;
            continue;
        }
        if (*reader == '\t') {
            do x += cw;
            while ((int)round(x - ox) % (cw * 4) != 0);
            ++ reader;
            continue;
        }
        if (*reader == '\n') {
            x = ox;
            y += ch * 5 / 4;
            ++ reader;
            continue;
        }

        if (width >= 0 && (nextspace - prevspace) * cw <= width && x + (nextspace - reader) * cw + cw - ox > width) 
            x = ox, y += ch * 5 / 4;
        
        buf.pos(x + cw * 3 / 2, y - ch / 2, 0); 
        buf.pos(x - cw / 2, y - ch / 2, 0); 
        buf.pos(x - cw / 2, y + ch * 3 / 2, 0);
        buf.pos(x - cw / 2, y + ch * 3 / 2, 0); 
        buf.pos(x + cw * 3 / 2, y + ch * 3 / 2, 0); 
        buf.pos(x + cw * 3 / 2, y - ch / 2, 0);

        int u = *reader % 16, v = *reader / 16;
        float lu = float(u) * 0.0625, lv = float(v) * 0.0625;

        for (int i = 0; i < 6; i ++)
            buf.col(red, green, blue, alpha),
            buf.norm(0, 0, -1),
            buf.spr(float(meta->x) / meta->w, float(meta->y) / meta->h, float(iw) / meta->w, float(ih) / meta->h);

        buf.uv(lu + 0.0625, lv); buf.uv(lu, lv); buf.uv(lu, lv + 0.0625);
        buf.uv(lu, lv + 0.0625); buf.uv(lu + 0.0625, lv + 0.0625); buf.uv(lu + 0.0625, lv);

        x += cw;
        ++ reader;
    }
}

static void normalize(float& x, float& y, float& z) {
    float len = sqrt(x * x + y * y + z * z);
    x /= len, y /= len, z /= len;
}

static void scaleduvtri(Buffer& buf, float u, float v, float w, float h, float iw, float ih, bool flipu, bool flipv) {
    if (flipu || flipv) buf.uv((u) / iw, (v + h) / ih);
    if (flipu || !flipv) buf.uv((u) / iw, (v) / ih);
    if (!flipu || !flipv) buf.uv((u + w) / iw, (v) / ih);
    if (!flipu || flipv) buf.uv((u + w) / iw, (v + h) / ih);
}

static void scaleduvxtri(Buffer& buf, float u, float v, float w, float h, float iw, float ih, bool flipu, bool flipv) {
    if (flipu || flipv) buf.uv((u + w) / iw, (v) / ih);
    if (flipu || !flipv) buf.uv((u) / iw, (v) / ih);
    if (!flipu || !flipv) buf.uv((u) / iw, (v + h) / ih);
    if (!flipu || flipv) buf.uv((u + w) / iw, (v + h) / ih);
}

static void autouvtri(Buffer& buf, float u, float v, float w, float h, float iw, float ih, bool flipu, bool flipv) {
    float u_auto = frem(u, iw) * 2 / iw, v_auto = frem(v, ih) * 2 / ih, w_auto = w / iw, h_auto = h / ih;
    if (flipu || flipv) buf.uv(u_auto, v_auto + h_auto);
    if (flipu || !flipv) buf.uv(u_auto, v_auto);
    if (!flipu || !flipv) buf.uv(u_auto + w_auto, v_auto);
    if (!flipu || flipv) buf.uv(u_auto + w_auto, v_auto + h_auto);
}

static void stretchuvtri(Buffer& buf, bool flipu, bool flipv) {
    if (flipu || flipv) buf.uv(0, 1);
    if (flipu || !flipv) buf.uv(0, 0);
    if (!flipu || !flipv) buf.uv(1, 0);
    if (!flipu || flipv) buf.uv(1, 1);
}

static void slant(Buffer& buf, float x, float y, float z, float w, float h, float l, Edge edge, Texture tex) {
    Image i = tex.img;
    bindtex(i);
    ImageMeta* meta = &findimg(i);
    while (meta->parent > 0) meta = &findimg(meta->parent);
    float dx = w / 2, dy = h / 2, dz = l / 2;
    float u = float(findimg(i).x) / meta->w, v = float(findimg(i).y) / meta->h;
    float iw = float(findimg(i).w), ih = float(findimg(i).h);
    float uw = float(findimg(i).w) / meta->w, vh = float(findimg(i).h) / meta->h;
    float ox = int(orig) % 3 - 1, oy = int(orig) % 9 / 3 - 1, oz = int(orig) / 9 - 1;
    x -= w * ox / 2; y -= h * oy / 2; z -= l * oz / 2;

    float nx = 0, ny = 0, nz = 0;

    switch (edge) {
        case TOP_LEFT_EDGE: {
            float hx = dx * 2, hy = dy * 2, hz = 0;
            float vx = 0, vy = 0, vz = dz * -2;
            nx = -(hy * vz - hz * vy), ny = -(hz * vx - hx * vz), nz = -(hx * vy - hy * vx);
            break;
        }
        case TOP_BACK_EDGE: {
            float hx = dx * 2, hy = 0, hz = 0;
            float vx = 0, vy = dy * 2, vz = dz * -2;
            nx = -(hy * vz - hz * vy), ny = -(hz * vx - hx * vz), nz = -(hx * vy - hy * vx);
            break;
        }
        case TOP_RIGHT_EDGE: {
            float hx = dx * 2, hy = dy * -2, hz = 0;
            float vx = 0, vy = 0, vz = dz * -2;
            nx = -(hy * vz - hz * vy), ny = -(hz * vx - hx * vz), nz = -(hx * vy - hy * vx);
            break;
        }
        case TOP_FRONT_EDGE: {
            float hx = dx * 2, hy = 0, hz = 0;
            float vx = 0, vy = dy * -2, vz = dz * -2;
            nx = -(hy * vz - hz * vy), ny = -(hz * vx - hx * vz), nz = -(hx * vy - hy * vx);
            break;
        }
        case FRONT_LEFT_EDGE: {
            float hx = dx * 2, hy = 0, hz = dz * -2;
            float vx = 0, vy = dy * -2, vz = 0;
            nx = -(hy * vz - hz * vy), ny = -(hz * vx - hx * vz), nz = -(hx * vy - hy * vx);
            break;
        }
        case FRONT_RIGHT_EDGE: {
            float hx = dx * -2, hy = 0, hz = dz * -2;
            float vx = 0, vy = dy * -2, vz = 0;
            nx = -(hy * vz - hz * vy), ny = -(hz * vx - hx * vz), nz = -(hx * vy - hy * vx);
            break;
        }
        case BACK_RIGHT_EDGE: {
            float hx = dx * -2, hy = 0, hz = dz * 2;
            float vx = 0, vy = dy * -2, vz = 0;
            nx = -(hy * vz - hz * vy), ny = -(hz * vx - hx * vz), nz = -(hx * vy - hy * vx);
            break;
        }
        case BACK_LEFT_EDGE: {
            float hx = dx * 2, hy = 0, hz = dz * 2;
            float vx = 0, vy = dy * -2, vz = 0;
            nx = -(hy * vz - hz * vy), ny = -(hz * vx - hx * vz), nz = -(hx * vy - hy * vx);
            break;
        }
        case BOTTOM_LEFT_EDGE: {
            float hx = dx * 2, hy = dy * 2, hz = 0;
            float vx = 0, vy = 0, vz = dz * 2;
            nx = -(hy * vz - hz * vy), ny = -(hz * vx - hx * vz), nz = -(hx * vy - hy * vx);
            break;
        }
        case BOTTOM_BACK_EDGE: {
            float hx = dx * 2, hy = 0, hz = 0;
            float vx = 0, vy = dy * 2, vz = dz * 2;
            nx = -(hy * vz - hz * vy), ny = -(hz * vx - hx * vz), nz = -(hx * vy - hy * vx);
            break;
        }
        case BOTTOM_RIGHT_EDGE: {
            float hx = dx * 2, hy = dy * -2, hz = 0;
            float vx = 0, vy = 0, vz = dz * 2;
            nx = -(hy * vz - hz * vy), ny = -(hz * vx - hx * vz), nz = -(hx * vy - hy * vx);
            break;
        }
        case BOTTOM_FRONT_EDGE: {
            float hx = dx * 2, hy = 0, hz = 0;
            float vx = 0, vy = dy * -2, vz = dz * 2;
            nx = -(hy * vz - hz * vy), ny = -(hz * vx - hx * vz), nz = -(hx * vy - hy * vx);
            break;
        }
    }
    normalize(nx, ny, nz);
    bool cullX = w > l + (edge % 2);

    switch (edge) {
        case TOP_LEFT_EDGE: {
            buf.pos(x - dx, y - dy, z - dz); buf.pos(x - dx, y - dy, z + dz); buf.pos(x - dx, y + dy, z + dz); // nx
            buf.pos(x - dx, y + dy, z + dz); buf.pos(x - dx, y + dy, z - dz); buf.pos(x - dx, y - dy, z - dz);
            buf.pos(x - dx, y - dy, z + dz); buf.pos(x - dx, y - dy, z - dz); buf.pos(x + dx, y + dy, z - dz); // ny
            buf.pos(x + dx, y + dy, z - dz); buf.pos(x + dx, y + dy, z + dz); buf.pos(x - dx, y - dy, z + dz);
            buf.pos(x - dx, y + dy, z - dz); buf.pos(x - dx, y + dy, z + dz); buf.pos(x + dx, y + dy, z + dz); // py
            buf.pos(x + dx, y + dy, z + dz); buf.pos(x + dx, y + dy, z - dz); buf.pos(x - dx, y + dy, z - dz);
            buf.pos(x - dx, y - dy, z - dz); buf.pos(x - dx, y + dy, z - dz); buf.pos(x + dx, y + dy, z - dz); // nz
            buf.pos(x + dx, y + dy, z + dz); buf.pos(x - dx, y + dy, z + dz); buf.pos(x - dx, y - dy, z + dz); // pz
            for (int i = 0; i < 6; i ++) buf.norm(-1, 0, 0);
            for (int i = 0; i < 6; i ++) buf.norm(nx, ny, nz);
            for (int i = 0; i < 6; i ++) buf.norm(0, 1, 0);
            for (int i = 0; i < 3; i ++) buf.norm(0, 0, -1);
            for (int i = 0; i < 3; i ++) buf.norm(0, 0, 1);

            switch (tex.type) {
                case TEX_NET:
                    scaleduvx(buf, h, h, -h, w, iw, ih); // negative x
                    scaleduv(buf, h * 2 + w, h, w, l, iw, ih); // negative y
                    scaleduv(buf, h, h, w, l, iw, ih); // positive y
                    scaleduvtri(buf, h + l, h, -l, -h, iw, ih, true, false);
                    scaleduvtri(buf, h, h + l, l, h, iw, ih, false, false);
                    break;
                case TEX_AUTO:
                    autouv(buf, z, y, l, h, iw, ih);
                    if (l * h > w * l) autouv(buf, z, y, l, h, iw, ih);
                    else autouv(buf, z, x, l, w, iw, ih);
                    autouv(buf, z, x, l, w, iw, ih);
                    autouvtri(buf, x, y, w, h, iw, ih, true, false);
                    autouvtri(buf, x, y, w, h, iw, ih, false, false);
                    break;
                case TEX_STRETCH:
                    stretchuv(buf);
                    stretchuv(buf);
                    stretchuv(buf);
                    stretchuvtri(buf, true, false);
                    stretchuvtri(buf, false, false);
                    break;
            }
            break;
        }
        case TOP_BACK_EDGE: {
            buf.pos(x - dx, y - dy, z + dz); buf.pos(x - dx, y + dy, z + dz); buf.pos(x - dx, y + dy, z - dz); // nx
            buf.pos(x + dx, y + dy, z - dz); buf.pos(x + dx, y + dy, z + dz); buf.pos(x + dx, y - dy, z + dz); // px
            buf.pos(x - dx, y + dy, z - dz); buf.pos(x - dx, y + dy, z + dz); buf.pos(x + dx, y + dy, z + dz); // py
            buf.pos(x + dx, y + dy, z + dz); buf.pos(x + dx, y + dy, z - dz); buf.pos(x - dx, y + dy, z - dz);
            buf.pos(x - dx, y - dy, z + dz); buf.pos(x - dx, y + dy, z - dz); buf.pos(x + dx, y + dy, z - dz); // ny
            buf.pos(x + dx, y + dy, z - dz); buf.pos(x + dx, y - dy, z + dz); buf.pos(x - dx, y - dy, z + dz);
            buf.pos(x - dx, y - dy, z + dz); buf.pos(x + dx, y - dy, z + dz); buf.pos(x + dx, y + dy, z + dz); // pz
            buf.pos(x + dx, y + dy, z + dz); buf.pos(x - dx, y + dy, z + dz); buf.pos(x - dx, y - dy, z + dz);
            for (int i = 0; i < 3; i ++) buf.norm(-1, 0, 0);
            for (int i = 0; i < 3; i ++) buf.norm(1, 0, 0);
            for (int i = 0; i < 6; i ++) buf.norm(0, 1, 0);
            for (int i = 0; i < 6; i ++) buf.norm(nx, ny, nz);
            for (int i = 0; i < 6; i ++) buf.norm(0, 0, 1);

            switch (tex.type) {
                case TEX_NET:
                    scaleduvxtri(buf, h, h, -h, w, iw, ih, true, false);
                    scaleduvxtri(buf, h + l, h + w, h, -w, iw, ih, false, false);
                    scaleduv(buf, h * 2 + w, h, w, l, iw, ih); // negative y
                    scaleduv(buf, h, h, w, l, iw, ih); // positive y
                    scaleduv(buf, h, h + l, l, h, iw, ih); // positive z
                    break;
                case TEX_AUTO:
                    autouvtri(buf, z, y, l, h, iw, ih, true, false);
                    autouvtri(buf, z, y, l, h, iw, ih, false, false);
                    autouv(buf, z, x, l, w, iw, ih);
                    if (w * h > w * l) autouv(buf, x, y, w, h, iw, ih);
                    else autouv(buf, z, x, l, w, iw, ih);
                    autouv(buf, x, y, w, h, iw, ih);
                    break;
                case TEX_STRETCH:
                    stretchuvtri(buf, true, false);
                    stretchuvtri(buf, false, false);
                    stretchuv(buf);
                    stretchuv(buf);
                    stretchuv(buf);
                    break;
            }
            break;
        }
        case TOP_RIGHT_EDGE: {
            buf.pos(x + dx, y - dy, z + dz); buf.pos(x + dx, y - dy, z - dz); buf.pos(x + dx, y + dy, z - dz); // px
            buf.pos(x + dx, y + dy, z - dz); buf.pos(x + dx, y + dy, z + dz); buf.pos(x + dx, y - dy, z + dz);
            buf.pos(x - dx, y + dy, z + dz); buf.pos(x - dx, y + dy, z - dz); buf.pos(x + dx, y - dy, z - dz); // ny
            buf.pos(x + dx, y - dy, z - dz); buf.pos(x + dx, y - dy, z + dz); buf.pos(x - dx, y + dy, z + dz);
            buf.pos(x - dx, y + dy, z - dz); buf.pos(x - dx, y + dy, z + dz); buf.pos(x + dx, y + dy, z + dz); // py
            buf.pos(x + dx, y + dy, z + dz); buf.pos(x + dx, y + dy, z - dz); buf.pos(x - dx, y + dy, z - dz); 
            buf.pos(x - dx, y + dy, z - dz); buf.pos(x + dx, y + dy, z - dz); buf.pos(x + dx, y - dy, z - dz); // nz
            buf.pos(x + dx, y - dy, z + dz); buf.pos(x + dx, y + dy, z + dz); buf.pos(x - dx, y + dy, z + dz); // pz
            for (int i = 0; i < 6; i ++) buf.norm(1, 0, 0);
            for (int i = 0; i < 6; i ++) buf.norm(nx, ny, nz);
            for (int i = 0; i < 6; i ++) buf.norm(0, 1, 0);
            for (int i = 0; i < 3; i ++) buf.norm(0, 0, -1);
            for (int i = 0; i < 3; i ++) buf.norm(0, 0, 1);

            switch (tex.type) {
                case TEX_NET:
                    scaleduvx(buf, h + l, h + w, h, -w, iw, ih); // positive x
                    scaleduv(buf, h * 2 + w, h, w, l, iw, ih); // negative y
                    scaleduv(buf, h, h, w, l, iw, ih); // positive y
                    scaleduvtri(buf, h + l, h, -l, -h, iw, ih, false, false);
                    scaleduvtri(buf, h, h + l, l, h, iw, ih, true, false);
                    break;
                case TEX_AUTO:
                    autouv(buf, z, y, l, h, iw, ih);
                    if (l * h > w * l) autouv(buf, z, y, l, h, iw, ih);
                    else autouv(buf, z, x, l, w, iw, ih);
                    autouv(buf, z, x, l, w, iw, ih);
                    autouvtri(buf, x, y, w, h, iw, ih, false, false);
                    autouvtri(buf, x, y, w, h, iw, ih, true, false);
                    break;
                case TEX_STRETCH:
                    stretchuv(buf);
                    stretchuv(buf);
                    stretchuv(buf);
                    stretchuvtri(buf, false, false);
                    stretchuvtri(buf, true, false);
                    break;
            }
            break;
        }
        case TOP_FRONT_EDGE: {
            buf.pos(x - dx, y + dy, z + dz); buf.pos(x - dx, y + dy, z - dz); buf.pos(x - dx, y - dy, z - dz); // nx
            buf.pos(x + dx, y - dy, z - dz); buf.pos(x + dx, y + dy, z - dz); buf.pos(x + dx, y + dy, z + dz); // px
            buf.pos(x - dx, y + dy, z - dz); buf.pos(x - dx, y + dy, z + dz); buf.pos(x + dx, y + dy, z + dz); // py
            buf.pos(x + dx, y + dy, z + dz); buf.pos(x + dx, y + dy, z - dz); buf.pos(x - dx, y + dy, z - dz);
            buf.pos(x - dx, y + dy, z + dz); buf.pos(x - dx, y - dy, z - dz); buf.pos(x + dx, y - dy, z - dz); // ny
            buf.pos(x + dx, y - dy, z - dz); buf.pos(x + dx, y + dy, z + dz); buf.pos(x - dx, y + dy, z + dz);
            buf.pos(x + dx, y - dy, z - dz); buf.pos(x - dx, y - dy, z - dz); buf.pos(x - dx, y + dy, z - dz); // nz
            buf.pos(x - dx, y + dy, z - dz); buf.pos(x + dx, y + dy, z - dz); buf.pos(x + dx, y - dy, z - dz);
            for (int i = 0; i < 3; i ++) buf.norm(-1, 0, 0);
            for (int i = 0; i < 3; i ++) buf.norm(1, 0, 0);
            for (int i = 0; i < 6; i ++) buf.norm(0, 1, 0);
            for (int i = 0; i < 6; i ++) buf.norm(nx, ny, nz);
            for (int i = 0; i < 6; i ++) buf.norm(0, 0, -1);

            switch (tex.type) {
                case TEX_NET:
                    scaleduvxtri(buf, h, h, -h, w, iw, ih, false, false);
                    scaleduvxtri(buf, h + l, h + w, h, -w, iw, ih, true, false);
                    scaleduv(buf, h * 2 + w, h, w, l, iw, ih); // negative y
                    scaleduv(buf, h, h, w, l, iw, ih); // positive y
                    scaleduv(buf, h + l, h, -l, -h, iw, ih); // negative z
                    break;
                case TEX_AUTO:
                    autouvtri(buf, z, y, l, h, iw, ih, false, false);
                    autouvtri(buf, z, y, l, h, iw, ih, true, false);
                    autouv(buf, z, x, l, w, iw, ih);
                    if (w * h > w * l) autouv(buf, x, y, w, h, iw, ih);
                    else autouv(buf, z, x, l, w, iw, ih);
                    autouv(buf, x, y, w, h, iw, ih);
                    break;
                case TEX_STRETCH:
                    stretchuvtri(buf, false, false);
                    stretchuvtri(buf, true, false);
                    stretchuv(buf);
                    stretchuv(buf);
                    stretchuv(buf);
                    break;
            }
            break;
        }
        case FRONT_LEFT_EDGE: {    
            buf.pos(x - dx, y - dy, z - dz); buf.pos(x - dx, y - dy, z + dz); buf.pos(x - dx, y + dy, z + dz); // nx
            buf.pos(x - dx, y + dy, z + dz); buf.pos(x - dx, y + dy, z - dz); buf.pos(x - dx, y - dy, z - dz);
            if (!cullX) {
                buf.pos(x - dx, y - dy, z + dz); buf.pos(x + dx, y - dy, z - dz); buf.pos(x + dx, y + dy, z - dz); // px
                buf.pos(x + dx, y + dy, z - dz); buf.pos(x - dx, y + dy, z + dz); buf.pos(x - dx, y - dy, z + dz);
            }
            buf.pos(x - dx, y - dy, z - dz); buf.pos(x + dx, y - dy, z - dz); buf.pos(x - dx, y - dy, z + dz); // ny
            buf.pos(x - dx, y + dy, z + dz); buf.pos(x + dx, y + dy, z - dz); buf.pos(x - dx, y + dy, z - dz); // py
            buf.pos(x + dx, y - dy, z - dz); buf.pos(x - dx, y - dy, z - dz); buf.pos(x - dx, y + dy, z - dz); // nz
            buf.pos(x - dx, y + dy, z - dz); buf.pos(x + dx, y + dy, z - dz); buf.pos(x + dx, y - dy, z - dz);
            if (cullX) {
                buf.pos(x - dx, y - dy, z + dz); buf.pos(x + dx, y - dy, z - dz); buf.pos(x + dx, y + dy, z - dz); // pz
                buf.pos(x + dx, y + dy, z - dz); buf.pos(x - dx, y + dy, z + dz); buf.pos(x - dx, y - dy, z + dz);
            }
            for (int i = 0; i < 6; i ++) buf.norm(-1, 0, 0);
            if (!cullX) for (int i = 0; i < 6; i ++) buf.norm(nx, ny, nz);
            for (int i = 0; i < 3; i ++) buf.norm(0, -1, 0);
            for (int i = 0; i < 3; i ++) buf.norm(0, 1, 0);
            for (int i = 0; i < 6; i ++) buf.norm(0, 0, -1);
            if (cullX) for (int i = 0; i < 6; i ++) buf.norm(nx, ny, nz);

            switch (tex.type) {
                case TEX_NET:
                    scaleduvx(buf, h, h, -h, w, iw, ih); // negative x
                    if (!cullX) scaleduvx(buf, h + l, h + w, h, -w, iw, ih); // positive x
                    scaleduvtri(buf, h * 2 + w, h, w, l, iw, ih, true, true); // negative y
                    scaleduvtri(buf, h, h, w, l, iw, ih, false, true); // positive y
                    scaleduv(buf, h + l, h, -l, -h, iw, ih); // negative z
                    if (cullX) scaleduv(buf, h, h + l, l, h, iw, ih); // positive z
                    break;
                case TEX_AUTO:
                    autouv(buf, z, y, l, h, iw, ih); // negative x
                    if (!cullX) autouv(buf, z, y, l, h, iw, ih); // positive x
                    autouvtri(buf, z, x, l, w, iw, ih, true, true); // negative y
                    autouvtri(buf, z, x, l, w, iw, ih, false, true); // positive y
                    autouv(buf, x, y, w, h, iw, ih); // negative z
                    if (cullX) autouv(buf, x, y, w, h, iw, ih); // positive z
                    break;
                case TEX_STRETCH:
                    stretchuv(buf);
                    if (!cullX) stretchuv(buf);
                    stretchuvtri(buf, true, true);
                    stretchuvtri(buf, false, true);
                    stretchuv(buf);
                    if (cullX) stretchuv(buf);
                    break;
            }
            break;
        }
        case FRONT_RIGHT_EDGE: {    
            if (!cullX) {
                buf.pos(x - dx, y - dy, z - dz); buf.pos(x + dx, y - dy, z + dz); buf.pos(x + dx, y + dy, z + dz); // nx
                buf.pos(x + dx, y + dy, z + dz); buf.pos(x - dx, y + dy, z - dz); buf.pos(x - dx, y - dy, z - dz);
            }
            buf.pos(x + dx, y - dy, z + dz); buf.pos(x + dx, y - dy, z - dz); buf.pos(x + dx, y + dy, z - dz); // px
            buf.pos(x + dx, y + dy, z - dz); buf.pos(x + dx, y + dy, z + dz); buf.pos(x + dx, y - dy, z + dz);
            buf.pos(x - dx, y - dy, z - dz); buf.pos(x + dx, y - dy, z - dz); buf.pos(x + dx, y - dy, z + dz); // ny
            buf.pos(x + dx, y + dy, z + dz); buf.pos(x + dx, y + dy, z - dz); buf.pos(x - dx, y + dy, z - dz); // py
            buf.pos(x + dx, y - dy, z - dz); buf.pos(x - dx, y - dy, z - dz); buf.pos(x - dx, y + dy, z - dz); // nz
            buf.pos(x - dx, y + dy, z - dz); buf.pos(x + dx, y + dy, z - dz); buf.pos(x + dx, y - dy, z - dz);
            if (cullX) {
                buf.pos(x - dx, y - dy, z - dz); buf.pos(x + dx, y - dy, z + dz); buf.pos(x + dx, y + dy, z + dz); // pz
                buf.pos(x + dx, y + dy, z + dz); buf.pos(x - dx, y + dy, z - dz); buf.pos(x - dx, y - dy, z - dz);
            }
            
            if (!cullX) for (int i = 0; i < 6; i ++) buf.norm(nx, ny, nz);
            for (int i = 0; i < 6; i ++) buf.norm(1, 0, 0);
            for (int i = 0; i < 3; i ++) buf.norm(0, -1, 0);
            for (int i = 0; i < 3; i ++) buf.norm(0, 1, 0);
            for (int i = 0; i < 6; i ++) buf.norm(0, 0, -1);
            if (cullX) for (int i = 0; i < 6; i ++) buf.norm(nx, ny, nz);

            switch (tex.type) {
                case TEX_NET:
                    if (!cullX) scaleduvx(buf, h, h, -h, w, iw, ih); // negative x
                    scaleduvx(buf, h + l, h + w, h, -w, iw, ih); // positive x
                    scaleduvtri(buf, h * 2 + w, h, w, l, iw, ih, true, false); // negative y
                    scaleduvtri(buf, h, h, w, l, iw, ih, false, false); // positive y
                    scaleduv(buf, h + l, h, -l, -h, iw, ih); // negative z
                    if (cullX) scaleduv(buf, h, h + l, l, h, iw, ih); // positive z
                    break;
                case TEX_AUTO:
                    if (!cullX) autouv(buf, z, y, l, h, iw, ih); // negative x
                    autouv(buf, z, y, l, h, iw, ih); // positive x
                    autouvtri(buf, z, x, l, w, iw, ih, true, false); // negative y
                    autouvtri(buf, z, x, l, w, iw, ih, false, false); // positive y
                    autouv(buf, x, y, w, h, iw, ih); // negative z
                    if (cullX) autouv(buf, x, y, w, h, iw, ih); // positive z
                    break;
                case TEX_STRETCH:
                    if (!cullX) stretchuv(buf);
                    stretchuv(buf);
                    stretchuvtri(buf, true, false);
                    stretchuvtri(buf, false, false);
                    stretchuv(buf);
                    if (cullX) stretchuv(buf);
                    break;
            }
            break;
        }
        case BACK_RIGHT_EDGE: {    
            if (!cullX) {
                buf.pos(x + dx, y - dy, z - dz); buf.pos(x - dx, y - dy, z + dz); buf.pos(x - dx, y + dy, z + dz); // nx
                buf.pos(x - dx, y + dy, z + dz); buf.pos(x + dx, y + dy, z - dz); buf.pos(x + dx, y - dy, z - dz);
            }
            buf.pos(x + dx, y - dy, z + dz); buf.pos(x + dx, y - dy, z - dz); buf.pos(x + dx, y + dy, z - dz); // px
            buf.pos(x + dx, y + dy, z - dz); buf.pos(x + dx, y + dy, z + dz); buf.pos(x + dx, y - dy, z + dz);
            buf.pos(x + dx, y - dy, z - dz); buf.pos(x + dx, y - dy, z + dz); buf.pos(x - dx, y - dy, z + dz); // ny
            buf.pos(x - dx, y + dy, z + dz); buf.pos(x + dx, y + dy, z + dz); buf.pos(x + dx, y + dy, z - dz); // py
            if (cullX) {
                buf.pos(x + dx, y - dy, z - dz); buf.pos(x - dx, y - dy, z + dz); buf.pos(x - dx, y + dy, z + dz); // nz
                buf.pos(x - dx, y + dy, z + dz); buf.pos(x + dx, y + dy, z - dz); buf.pos(x + dx, y - dy, z - dz);
            }
            buf.pos(x - dx, y - dy, z + dz); buf.pos(x + dx, y - dy, z + dz); buf.pos(x + dx, y + dy, z + dz); // pz
            buf.pos(x + dx, y + dy, z + dz); buf.pos(x - dx, y + dy, z + dz); buf.pos(x - dx, y - dy, z + dz);
            
            if (!cullX) for (int i = 0; i < 6; i ++) buf.norm(nx, ny, nz);
            for (int i = 0; i < 6; i ++) buf.norm(1, 0, 0);
            for (int i = 0; i < 3; i ++) buf.norm(0, -1, 0);
            for (int i = 0; i < 3; i ++) buf.norm(0, 1, 0);
            if (cullX) for (int i = 0; i < 6; i ++) buf.norm(nx, ny, nz);
            for (int i = 0; i < 6; i ++) buf.norm(0, 0, 1);

            switch (tex.type) {
                case TEX_NET:
                    if (!cullX) scaleduvx(buf, h, h, -h, w, iw, ih); // negative x
                    scaleduvx(buf, h + l, h + w, h, -w, iw, ih); // positive x
                    scaleduvtri(buf, h * 2 + w, h, w, l, iw, ih, false, false); // negative y
                    scaleduvtri(buf, h, h, w, l, iw, ih, true, false); // positive y
                    if (cullX) scaleduv(buf, h + l, h, -l, -h, iw, ih); // negative z
                    scaleduv(buf, h, h + l, l, h, iw, ih); // positive z
                    break;
                case TEX_AUTO:
                    if (!cullX) autouv(buf, z, y, l, h, iw, ih); // negative x
                    autouv(buf, z, y, l, h, iw, ih); // positive x
                    autouvtri(buf, z, x, l, w, iw, ih, false, false); // negative y
                    autouvtri(buf, z, x, l, w, iw, ih, true, false); // positive y
                    if (cullX) autouv(buf, x, y, w, h, iw, ih); // negative z
                    autouv(buf, x, y, w, h, iw, ih); // positive z
                    break;
                case TEX_STRETCH:
                    if (!cullX) stretchuv(buf);
                    stretchuv(buf);
                    stretchuvtri(buf, false, false);
                    stretchuvtri(buf, true, false);
                    if (cullX) stretchuv(buf);
                    stretchuv(buf);
                    break;
            }
            break;
        }
        case BACK_LEFT_EDGE: {    
            buf.pos(x - dx, y - dy, z - dz); buf.pos(x - dx, y - dy, z + dz); buf.pos(x - dx, y + dy, z + dz); // nx
            buf.pos(x - dx, y + dy, z + dz); buf.pos(x - dx, y + dy, z - dz); buf.pos(x - dx, y - dy, z - dz);
            if (!cullX) {
                buf.pos(x + dx, y - dy, z + dz); buf.pos(x - dx, y - dy, z - dz); buf.pos(x - dx, y + dy, z - dz); // px
                buf.pos(x - dx, y + dy, z - dz); buf.pos(x + dx, y + dy, z + dz); buf.pos(x + dx, y - dy, z + dz);
            }
            buf.pos(x - dx, y - dy, z - dz); buf.pos(x + dx, y - dy, z + dz); buf.pos(x - dx, y - dy, z + dz); // ny
            buf.pos(x - dx, y + dy, z + dz); buf.pos(x + dx, y + dy, z + dz); buf.pos(x - dx, y + dy, z - dz); // py
            if (cullX) {
                buf.pos(x + dx, y - dy, z + dz); buf.pos(x - dx, y - dy, z - dz); buf.pos(x - dx, y + dy, z - dz); // nz
                buf.pos(x - dx, y + dy, z - dz); buf.pos(x + dx, y + dy, z + dz); buf.pos(x + dx, y - dy, z + dz);
            }
            buf.pos(x - dx, y - dy, z + dz); buf.pos(x + dx, y - dy, z + dz); buf.pos(x + dx, y + dy, z + dz); // pz
            buf.pos(x + dx, y + dy, z + dz); buf.pos(x - dx, y + dy, z + dz); buf.pos(x - dx, y - dy, z + dz);
            
            for (int i = 0; i < 6; i ++) buf.norm(-1, 0, 0);
            if (!cullX) for (int i = 0; i < 6; i ++) buf.norm(nx, ny, nz);
            for (int i = 0; i < 3; i ++) buf.norm(0, -1, 0);
            for (int i = 0; i < 3; i ++) buf.norm(0, 1, 0);
            if (cullX) for (int i = 0; i < 6; i ++) buf.norm(nx, ny, nz);
            for (int i = 0; i < 6; i ++) buf.norm(0, 0, 1);

            switch (tex.type) {
                case TEX_NET:
                    scaleduvx(buf, h, h, -h, w, iw, ih); // negative x
                    if (!cullX) scaleduvx(buf, h + l, h + w, h, -w, iw, ih); // positive x
                    scaleduvtri(buf, h * 2 + w, h, w, l, iw, ih, false, true); // negative y
                    scaleduvtri(buf, h, h, w, l, iw, ih, true, true); // positive y
                    if (cullX) scaleduv(buf, h + l, h, -l, -h, iw, ih); // negative z
                    scaleduv(buf, h, h + l, l, h, iw, ih); // positive z
                    break;
                case TEX_AUTO:
                    autouv(buf, z, y, l, h, iw, ih); // negative x
                    if (!cullX) autouv(buf, z, y, l, h, iw, ih); // positive x
                    autouvtri(buf, z, x, l, w, iw, ih, false, true); // negative y
                    autouvtri(buf, z, x, l, w, iw, ih, true, true); // positive y
                    if (cullX) autouv(buf, x, y, w, h, iw, ih); // negative z
                    autouv(buf, x, y, w, h, iw, ih); // positive z
                    break;
                case TEX_STRETCH:
                    stretchuv(buf);
                    if (!cullX) stretchuv(buf);
                    stretchuvtri(buf, false, true);
                    stretchuvtri(buf, true, true);
                    if (cullX) stretchuv(buf);
                    stretchuv(buf);
                    break;
            }
            break;
        }
        case BOTTOM_LEFT_EDGE: {
            buf.pos(x - dx, y - dy, z - dz); buf.pos(x - dx, y - dy, z + dz); buf.pos(x - dx, y + dy, z + dz); // nx
            buf.pos(x - dx, y + dy, z + dz); buf.pos(x - dx, y + dy, z - dz); buf.pos(x - dx, y - dy, z - dz);
            buf.pos(x - dx, y + dy, z - dz); buf.pos(x - dx, y + dy, z + dz); buf.pos(x + dx, y - dy, z + dz); // py
            buf.pos(x + dx, y - dy, z + dz); buf.pos(x + dx, y - dy, z - dz); buf.pos(x - dx, y + dy, z - dz);
            buf.pos(x - dx, y - dy, z + dz); buf.pos(x - dx, y - dy, z - dz); buf.pos(x + dx, y - dy, z - dz); // ny
            buf.pos(x + dx, y - dy, z - dz); buf.pos(x + dx, y - dy, z + dz); buf.pos(x - dx, y - dy, z + dz);
            buf.pos(x - dx, y - dy, z - dz); buf.pos(x - dx, y + dy, z - dz); buf.pos(x + dx, y - dy, z - dz); // nz
            buf.pos(x + dx, y - dy, z + dz); buf.pos(x - dx, y + dy, z + dz); buf.pos(x - dx, y - dy, z + dz); // pz
            for (int i = 0; i < 6; i ++) buf.norm(-1, 0, 0);
            for (int i = 0; i < 6; i ++) buf.norm(nx, ny, nz);
            for (int i = 0; i < 6; i ++) buf.norm(0, -1, 0);
            for (int i = 0; i < 3; i ++) buf.norm(0, 0, -1);
            for (int i = 0; i < 3; i ++) buf.norm(0, 0, 1);

            switch (tex.type) {
                case TEX_NET:
                    scaleduvx(buf, h, h, -h, w, iw, ih); // negative x
                    scaleduv(buf, h * 2 + w, h, w, l, iw, ih); // negative y
                    scaleduv(buf, h, h, w, l, iw, ih); // positive y
                    scaleduvtri(buf, h + l, h, -l, -h, iw, ih, true, true);
                    scaleduvtri(buf, h, h + l, l, h, iw, ih, false, true);
                    break;
                case TEX_AUTO:
                    autouv(buf, z, y, l, h, iw, ih);
                    if (l * h > w * h) autouv(buf, z, y, l, h, iw, ih);
                    else autouv(buf, z, x, l, w, iw, ih);
                    autouv(buf, z, x, l, w, iw, ih);
                    autouvtri(buf, x, y, w, h, iw, ih, true, true);
                    autouvtri(buf, x, y, w, h, iw, ih, false, true);
                    break;
                case TEX_STRETCH:
                    stretchuv(buf);
                    stretchuv(buf);
                    stretchuv(buf);
                    stretchuvtri(buf, true, true);
                    stretchuvtri(buf, false, true);
                    break;
            }
            break;
        }
        case BOTTOM_BACK_EDGE: {
            buf.pos(x - dx, y - dy, z + dz); buf.pos(x - dx, y + dy, z + dz); buf.pos(x - dx, y - dy, z - dz); // nx
            buf.pos(x + dx, y - dy, z - dz); buf.pos(x + dx, y + dy, z + dz); buf.pos(x + dx, y - dy, z + dz); // px
            buf.pos(x - dx, y - dy, z + dz); buf.pos(x - dx, y - dy, z - dz); buf.pos(x + dx, y - dy, z - dz); // ny
            buf.pos(x + dx, y - dy, z - dz); buf.pos(x + dx, y - dy, z + dz); buf.pos(x - dx, y - dy, z + dz);
            buf.pos(x - dx, y - dy, z - dz); buf.pos(x - dx, y + dy, z + dz); buf.pos(x + dx, y + dy, z + dz); // py
            buf.pos(x + dx, y + dy, z + dz); buf.pos(x + dx, y - dy, z - dz); buf.pos(x - dx, y - dy, z - dz);
            buf.pos(x - dx, y - dy, z + dz); buf.pos(x + dx, y - dy, z + dz); buf.pos(x + dx, y + dy, z + dz); // pz
            buf.pos(x + dx, y + dy, z + dz); buf.pos(x - dx, y + dy, z + dz); buf.pos(x - dx, y - dy, z + dz);
            for (int i = 0; i < 3; i ++) buf.norm(-1, 0, 0);
            for (int i = 0; i < 3; i ++) buf.norm(1, 0, 0);
            for (int i = 0; i < 6; i ++) buf.norm(0, -1, 0);
            for (int i = 0; i < 6; i ++) buf.norm(nx, ny, nz);
            for (int i = 0; i < 6; i ++) buf.norm(0, 0, 1);

            switch (tex.type) {
                case TEX_NET:
                    scaleduvxtri(buf, h, h, -h, w, iw, ih, true, true);
                    scaleduvxtri(buf, h + l, h + w, h, -w, iw, ih, false, true);
                    scaleduv(buf, h * 2 + w, h, w, l, iw, ih); // negative y
                    scaleduv(buf, h, h, w, l, iw, ih); // positive y
                    scaleduv(buf, h, h + l, l, h, iw, ih); // positive z
                    break;
                case TEX_AUTO:
                    autouvtri(buf, z, y, l, h, iw, ih, true, true);
                    autouvtri(buf, z, y, l, h, iw, ih, false, true);
                    autouv(buf, z, x, l, w, iw, ih);
                    if (w * h > w * l) autouv(buf, x, y, w, h, iw, ih);
                    else autouv(buf, z, x, l, w, iw, ih);
                    autouv(buf, x, y, w, h, iw, ih);
                    break;
                case TEX_STRETCH:
                    stretchuvtri(buf, true, true);
                    stretchuvtri(buf, false, true);
                    stretchuv(buf);
                    stretchuv(buf);
                    stretchuv(buf);
                    break;
            }
            break;
        }
        case BOTTOM_RIGHT_EDGE: {
            buf.pos(x + dx, y - dy, z + dz); buf.pos(x + dx, y - dy, z - dz); buf.pos(x + dx, y + dy, z - dz); // px
            buf.pos(x + dx, y + dy, z - dz); buf.pos(x + dx, y + dy, z + dz); buf.pos(x + dx, y - dy, z + dz);
            buf.pos(x - dx, y - dy, z - dz); buf.pos(x - dx, y - dy, z + dz); buf.pos(x + dx, y + dy, z + dz); // py
            buf.pos(x + dx, y + dy, z + dz); buf.pos(x + dx, y + dy, z - dz); buf.pos(x - dx, y - dy, z - dz);
            buf.pos(x - dx, y - dy, z + dz); buf.pos(x - dx, y - dy, z - dz); buf.pos(x + dx, y - dy, z - dz); // ny
            buf.pos(x + dx, y - dy, z - dz); buf.pos(x + dx, y - dy, z + dz); buf.pos(x - dx, y - dy, z + dz);
            buf.pos(x - dx, y - dy, z - dz); buf.pos(x + dx, y + dy, z - dz); buf.pos(x + dx, y - dy, z - dz); // nz
            buf.pos(x + dx, y - dy, z + dz); buf.pos(x + dx, y + dy, z + dz); buf.pos(x - dx, y - dy, z + dz); // pz
            for (int i = 0; i < 6; i ++) buf.norm(1, 0, 0);
            for (int i = 0; i < 6; i ++) buf.norm(nx, ny, nz);
            for (int i = 0; i < 6; i ++) buf.norm(0, -1, 0);
            for (int i = 0; i < 3; i ++) buf.norm(0, 0, -1);
            for (int i = 0; i < 3; i ++) buf.norm(0, 0, 1);

            switch (tex.type) {
                case TEX_NET:
                    scaleduvx(buf, h + l, h + w, h, -w, iw, ih); // positive x
                    scaleduv(buf, h * 2 + w, h, w, l, iw, ih); // negative y
                    scaleduv(buf, h, h, w, l, iw, ih); // positive y
                    scaleduvtri(buf, h + l, h, -l, -h, iw, ih, false, true);
                    scaleduvtri(buf, h, h + l, l, h, iw, ih, true, true);
                    break;
                case TEX_AUTO:
                    autouv(buf, z, y, l, h, iw, ih);
                    if (l * h > w * h) autouv(buf, z, y, l, h, iw, ih);
                    else autouv(buf, z, x, l, w, iw, ih);
                    autouv(buf, z, x, l, w, iw, ih);
                    autouvtri(buf, x, y, w, h, iw, ih, false, true);
                    autouvtri(buf, x, y, w, h, iw, ih, true, true);
                    break;
                case TEX_STRETCH:
                    stretchuv(buf);
                    stretchuv(buf);
                    stretchuv(buf);
                    stretchuvtri(buf, false, true);
                    stretchuvtri(buf, true, true);
                    break;
            }
            break;
        }
        case BOTTOM_FRONT_EDGE: {
            buf.pos(x - dx, y - dy, z + dz); buf.pos(x - dx, y + dy, z - dz); buf.pos(x - dx, y - dy, z - dz); // nx
            buf.pos(x + dx, y - dy, z - dz); buf.pos(x + dx, y + dy, z - dz); buf.pos(x + dx, y - dy, z + dz); // px
            buf.pos(x - dx, y - dy, z + dz); buf.pos(x - dx, y - dy, z - dz); buf.pos(x + dx, y - dy, z - dz); // ny
            buf.pos(x + dx, y - dy, z - dz); buf.pos(x + dx, y - dy, z + dz); buf.pos(x - dx, y - dy, z + dz);
            buf.pos(x - dx, y + dy, z - dz); buf.pos(x - dx, y - dy, z + dz); buf.pos(x + dx, y - dy, z + dz); // py
            buf.pos(x + dx, y - dy, z + dz); buf.pos(x + dx, y + dy, z - dz); buf.pos(x - dx, y + dy, z - dz);
            buf.pos(x + dx, y - dy, z - dz); buf.pos(x - dx, y - dy, z - dz); buf.pos(x - dx, y + dy, z - dz); // nz
            buf.pos(x - dx, y + dy, z - dz); buf.pos(x + dx, y + dy, z - dz); buf.pos(x + dx, y - dy, z - dz);
            for (int i = 0; i < 3; i ++) buf.norm(-1, 0, 0);
            for (int i = 0; i < 3; i ++) buf.norm(1, 0, 0);
            for (int i = 0; i < 6; i ++) buf.norm(0, -1, 0);
            for (int i = 0; i < 6; i ++) buf.norm(nx, ny, nz);
            for (int i = 0; i < 6; i ++) buf.norm(0, 0, -1);

            switch (tex.type) {
                case TEX_NET:
                    scaleduvxtri(buf, h, h, -h, w, iw, ih, false, true);
                    scaleduvxtri(buf, h + l, h + w, h, -w, iw, ih, true, true);
                    scaleduv(buf, h * 2 + w, h, w, l, iw, ih); // negative y
                    scaleduv(buf, h, h, w, l, iw, ih); // positive y
                    scaleduv(buf, h + l, h, -l, -h, iw, ih); // negative z
                    break;
                case TEX_AUTO:
                    autouvtri(buf, z, y, l, h, iw, ih, false, true);
                    autouvtri(buf, z, y, l, h, iw, ih, true, true);
                    autouv(buf, z, x, l, w, iw, ih);
                    if (w * h > w * l) autouv(buf, x, y, w, h, iw, ih);
                    else autouv(buf, z, x, l, w, iw, ih);
                    autouv(buf, x, y, w, h, iw, ih);
                    break;
                case TEX_STRETCH:
                    stretchuvtri(buf, false, true);
                    stretchuvtri(buf, true, true);
                    stretchuv(buf);
                    stretchuv(buf);
                    stretchuv(buf);
                    break;
            }
            break;
        }
    }

    for (int i = 0; i < 24; i ++) buf.col(red, green, blue, alpha);
    for (int i = 0; i < 24; i ++) buf.spr(u, v, uw, vh);
}

void ensure2d() {
    if (mode3d) {
        mode3d = false;
        glUniform3f(find_uniform("light"), 0, 0, 1);
        glDepthMask(GL_FALSE);
    }
}

void ensure3d() {
    if (!mode3d) {
        mode3d = true;
        glUniform3f(find_uniform("light"), lightx, lighty, lightz);
        glDepthMask(GL_TRUE);
    }
}

static void recalc_boards() {
    boardh[0] = sin((90 - yaw) * pi / 180);
    boardh[1] = 0;
    boardh[2] = cos((90 - yaw) * pi / 180);

    boardv[0] = sin(-yaw * pi / 180) * cos((-90 - pitch) * pi / 180);
    boardv[1] = sin((-90 - pitch) * pi / 180);
    boardv[2] = cos(-yaw * pi / 180) * cos((-90 - pitch) * pi / 180);
}

static void step(Buffer& buf, const Step& step) {
    switch (step.type) {
        case STEP_SET_COLOR: {
            Color c = step.data.set_color.color;
            red = (c >> 24 & 255) / 255.0f;
            green = (c >> 16 & 255) / 255.0f;
            blue = (c >> 8 & 255) / 255.0f;
            alpha = (c & 255) / 255.0f;
            return;
        }
        case STEP_SET_ORIGIN: {
            orig = step.data.set_origin.origin;
            return;
        }
        case STEP_RECT: {
            ensure2d();
            auto& r = step.data.rect;
            Origin o = orig;
            orig = TOP_LEFT;
            plane(buf, r.x, r.y, 0, r.w, r.h, 1, 0, 0, 0, 1, 0, BLANK);
            orig = o;
            return;
        }
        case STEP_POLYGON: {
            ensure2d();
            auto& p = step.data.polygon;
            polygon(buf, p.x, p.y, p.r, p.n);
            return;
        }
        case STEP_SPRITE: {
            ensure2d();
            auto& r = step.data.sprite;
            return plane(buf, r.x, r.y, 0, r.w, r.h, 1, 0, 0, 0, 1, 0, r.img);
        }
        case STEP_FONT: {
            currentfont = step.data.font.img;
            return;
        }
        case STEP_TEXT: {
            ensure2d();
            auto& t = step.data.text;
            text(buf, t.x, t.y, t.str, -1);
            delete[] t.str;
            return;
        }
        case STEP_WRAPPED_TEXT: {
            ensure2d();
            auto& t  = step.data.wraptext;
            text(buf, t.x, t.y, t.str, t.width);
            delete[] t.str;
            return;
        }
        case STEP_CUBE: {
            ensure3d();
            auto& c = step.data.cube;
            return cube(buf, c.x, c.y, c.z, c.w, c.h, c.l, c.tex);
        }
        case STEP_BOARD: {
            ensure3d();
            auto& b = step.data.board;
            return plane(buf, b.x, b.y, b.z, b.w, b.h, boardh[0], boardh[1], boardh[2], boardv[0], boardv[1], boardv[2], b.img);
        }
        case STEP_SLANT: {
            ensure3d();
            auto& c = step.data.slant;
            return slant(buf, c.x, c.y, c.z, c.w, c.h, c.l, c.edge, c.tex);
        }
        case STEP_PRISM: {
            ensure3d();
            auto& p = step.data.prism;
            return prism(buf, p.x, p.y, p.z, p.w, p.h, p.l, p.n, p.axis, p.tex);
        }
        case STEP_CONE: {
            ensure3d();
            auto& c = step.data.cone;
            return pyramid(buf, c.x, c.y, c.z, c.w, c.h, c.l, c.n, c.dir, c.tex);
        }
        case STEP_HEDRON: {
            ensure3d();
            auto& h = step.data.hedron;
            return hedron(buf, h.x, h.y, h.z, h.w, h.h, h.l, h.m, h.n, h.tex);
        }
        case STEP_ORTHO: {
            identity(projection);
            ortho(projection, step.data.ortho.w, step.data.ortho.h);
            glUniformMatrix4fv(find_uniform("projection"), 1, GL_FALSE, (const GLfloat*)projection);
            return;
        }
        case STEP_FRUSTUM: {
            identity(projection);
            frustum(projection, step.data.frustum.w, step.data.frustum.h, step.data.frustum.fov);
            glUniformMatrix4fv(find_uniform("projection"), 1, GL_FALSE, (const GLfloat*)projection);
            return;
        }
        case STEP_PAN: {
            translate(view, step.data.pan.x, step.data.pan.y, step.data.pan.z);
            glUniformMatrix4fv(find_uniform("view"), 1, GL_FALSE, (const GLfloat*)view);
            return;
        }
        case STEP_TILT: {
            switch (step.data.tilt.axis) {
                case X_AXIS:
                    rotatex(view, step.data.tilt.degrees);
                    break;
                case Y_AXIS:
                    rotatey(view, step.data.tilt.degrees);
                    break;
                case Z_AXIS:
                    rotatez(view, step.data.tilt.degrees);
                    break;
            }
            glUniformMatrix4fv(find_uniform("view"), 1, GL_FALSE, (const GLfloat*)view);
            return;
        }
        case STEP_LOOK: {
            identity(view);
            camerax = step.data.look.x, cameray = step.data.look.y, cameraz = step.data.look.z;
            yaw = step.data.look.yaw, pitch = step.data.look.pitch;
            recalc_boards();
            translate(view, -step.data.look.x, -step.data.look.y, -step.data.look.z);
            rotatey(view, yaw);
            rotatex(view, pitch);
            glUniformMatrix4fv(find_uniform("view"), 1, GL_FALSE, (const GLfloat*)view);
            return;
        }
        case STEP_ROTATE: {
            switch (step.data.rotate.axis) {
                case X_AXIS:
                    rotatex(transform, step.data.rotate.angle);
                    break;
                case Y_AXIS:
                    rotatey(transform, step.data.rotate.angle);
                    break;
                case Z_AXIS:
                    rotatez(transform, step.data.rotate.angle);
                    break;
            }
            glUniformMatrix4fv(find_uniform("model"), 1, GL_FALSE, (const GLfloat*)transform);
            return;
        }
        case STEP_SCALE: {
            scale(transform, step.data.scale.x, step.data.scale.y, step.data.scale.z);
            glUniformMatrix4fv(find_uniform("model"), 1, GL_FALSE, (const GLfloat*)transform);
            return;
        }
        case STEP_TRANSLATE: {
            translate(transform, step.data.translate.x, step.data.translate.y, step.data.translate.z);
            glUniformMatrix4fv(find_uniform("model"), 1, GL_FALSE, (const GLfloat*)transform);
            return;
        }
        case STEP_RENDER: {
            ensure3d();
            bindtex(step.data.render.img);
            drawbuf(findbuf(step.data.render.model));
            return;
        }
        case STEP_BEGIN: {
            mat4 m;
            matset(m.data, transform);
            matstack.push(m);
            return;
        }
        case STEP_END: {
            if (matstack.size() == 0) println("Tried to end transform sequence, but matrix stack was empty!"), exit(1);
            matset(transform, matstack.back().data);
            glUniformMatrix4fv(find_uniform("model"), 1, GL_FALSE, (const GLfloat*)transform);
            matstack.pop();
            return;
        }
        case STEP_FOG: {
            Color c = step.data.fog.color;
            float red = (c >> 24 & 255) / 255.0f;
            float green = (c >> 16 & 255) / 255.0f;
            float blue = (c >> 8 & 255) / 255.0f;
            float alpha = (c & 255) / 255.0f;
            glUniform4f(find_uniform("fog_color"), red, green, blue, alpha);
            glUniform1f(find_uniform("fog_range"), step.data.fog.range);
            return;
        }
    }
}

void flush(Model model) {
    Buffer& buf = findbuf(model);
    for (const Step& step : steps) {
        if (stateful(step)) drawbuf(buf), buf.reset();
        ::step(buf, step);
    }
    steps.clear();
    drawbuf(buf);
    buf.reset();
}

void init_queue() {
    rendermodel = init_render_buffer();
}

Model getrendermodel() {
    return rendermodel;
}

// Colors

extern "C" void LIBDRAW_SYMBOL(color)(Color c) {
    Step step;
    step.type = STEP_SET_COLOR;
    step.data.set_color = { c };
    enqueue(step);
}

// 2D Drawing

extern "C" void LIBDRAW_SYMBOL(rect)(float x, float y, float w, float h) {
    Step step;
    step.type = STEP_RECT;
    step.data.rect = { x, y, w, h };
    enqueue(step);
}

extern "C" void LIBDRAW_SYMBOL(polygon)(float x, float y, float r, int n) {
    Step step;
    step.type = STEP_POLYGON;
    step.data.polygon = { x, y, r, n };
    enqueue(step);
}

extern "C" void LIBDRAW_SYMBOL(circle)(float x, float y, float r) {
    polygon(x, y, r, 64);
}

extern "C" void LIBDRAW_SYMBOL(sprite)(float x, float y, Image img) {
    Step step;
    step.type = STEP_SPRITE;
    step.data.sprite = { x, y, (float)width(img), (float)height(img), img };
    enqueue(step);
}

void stretched_sprite(float x, float y, float w, float h, Image img) {
    Step step;
    step.type = STEP_SPRITE;
    step.data.sprite = { x, y, w, h, img };
    enqueue(step);
}

extern "C" void LIBDRAW_SYMBOL(font)(Image i) {
    Step step;
    step.type = STEP_FONT;
    step.data.font = { i };
    enqueue(step);
}

extern "C" void LIBDRAW_SYMBOL(text)(float x, float y, const char* str) {
    Step step;
    step.type = STEP_TEXT;
    step.data.text = { x, y, strdup(str) };
    enqueue(step);
}

extern "C" void LIBDRAW_SYMBOL(wraptext)(float x, float y, const char* str, int width) {
    Step step;
    step.type = STEP_WRAPPED_TEXT;
    step.data.wraptext = { x, y, strdup(str), float(width) };
    enqueue(step);
}

// 3D Drawing

extern "C" Texture LIBDRAW_SYMBOL(nettex)(Image img) {
    return {img, LIBDRAW_CONST(TEX_NET)};
}

extern "C" Texture LIBDRAW_SYMBOL(autotex)(Image img) {
    return {img, LIBDRAW_CONST(TEX_AUTO)};
}

extern "C" Texture LIBDRAW_SYMBOL(stretchtex)(Image img) {
    return {img, LIBDRAW_CONST(TEX_STRETCH)};
}

extern "C" void LIBDRAW_SYMBOL(cube)(float x, float y, float z, float w, float h, float l, Texture img) {
    Step step;
    step.type = STEP_CUBE;
    step.data.cube = { x, y, z, w, h, l, img };
    enqueue(step);
}

extern "C" void LIBDRAW_SYMBOL(board)(float x, float y, float z, Image img) {
    Step step;
    step.type = STEP_BOARD;
    step.data.board = { x, y, z, (float)width(img), (float)height(img), img };
    enqueue(step);
}

extern "C" void LIBDRAW_SYMBOL(slant)(float x, float y, float z, float w, float h, float l, Edge edge, Texture img) {
    Step step;
    step.type = STEP_SLANT;
    step.data.slant = { x, y, z, w, h, l, edge, img };
    enqueue(step);
}

extern "C" void LIBDRAW_SYMBOL(prism)(float x, float y, float z, float w, float h, float l, int n, Axis axis, Texture img) {
    Step step;
    step.type = STEP_PRISM;
    step.data.prism = { x, y, z, w, h, l, n, axis, img };
    enqueue(step);
}

extern "C" void LIBDRAW_SYMBOL(cylinder)(float x, float y, float z, float w, float h, float l, Axis axis, Texture img) {
    prism(x, y, z, w, h, l, 32, axis, img);
}

extern "C" void LIBDRAW_SYMBOL(pyramid)(float x, float y, float z, float w, float h, float l, int n, Direction dir, Texture img) {
    Step step;
    step.type = STEP_CONE;
    step.data.cone = { x, y, z, w, h, l, n, dir, img };
    enqueue(step);
}

extern "C" void LIBDRAW_SYMBOL(cone)(float x, float y, float z, float w, float h, float l, Direction dir, Texture img) {
    pyramid(x, y, z, w, h, l, 32, dir, img);
}

extern "C" void LIBDRAW_SYMBOL(hedron)(float x, float y, float z, float w, float h, float l, int m, int n, Texture img) {
    Step step;
    step.type = STEP_HEDRON;
    step.data.hedron = { x, y, z, w, h, l, m, n, img };
    enqueue(step);
}

extern "C" void LIBDRAW_SYMBOL(sphere)(float x, float y, float z, float w, float h, float l, Texture img) {
    hedron(x, y, z, w, h, l, 16, 8, img);
}

// Transformation

extern "C" void LIBDRAW_SYMBOL(origin)(Origin origin) {
    Step step;
    step.type = STEP_SET_ORIGIN;
    step.data.set_origin = { origin };
    enqueue(step);
}

extern "C" void LIBDRAW_SYMBOL(rotate)(float degrees, Axis axis) {
    Step step;
    step.type = STEP_ROTATE;
    step.data.rotate = { degrees, axis };
    enqueue(step);
}

extern "C" void LIBDRAW_SYMBOL(translate)(float dx, float dy, float dz) {
    Step step;
    step.type = STEP_TRANSLATE;
    step.data.translate = { dx, dy, dz };
    enqueue(step);
}

extern "C" void LIBDRAW_SYMBOL(scale)(float factor) {
    Step step;
    step.type = STEP_SCALE;
    step.data.scale = { factor, factor, factor };
    enqueue(step);
}

extern "C" void LIBDRAW_SYMBOL(scale2d)(float x, float y) {
    Step step;
    step.type = STEP_SCALE;
    step.data.scale = { x, y, 1 };
    enqueue(step);
}

extern "C" void LIBDRAW_SYMBOL(scale3d)(float x, float y, float z) {
    Step step;
    step.type = STEP_SCALE;
    step.data.scale = { x, y, z };
    enqueue(step);
}

extern "C" void LIBDRAW_SYMBOL(beginstate)() {
    Step step;
    step.type = STEP_BEGIN;
    enqueue(step);
}

extern "C" void LIBDRAW_SYMBOL(endstate)() {
    Step step;
    step.type = STEP_END;
    enqueue(step);
}

// Camera

extern "C" void LIBDRAW_SYMBOL(ortho)(float w, float h) {
    Step step;
    step.type = STEP_ORTHO;
    step.data.ortho = { w, h };
    enqueue(step);
}

extern "C" void LIBDRAW_SYMBOL(frustum)(float w, float h, float fov) {
    Step step;
    step.type = STEP_FRUSTUM;
    step.data.frustum = { w, h, fov / 2 };
    enqueue(step);
}

extern "C" void LIBDRAW_SYMBOL(pan)(float x, float y, float z) {
    Step step;
    step.type = STEP_PAN;
    step.data.pan = { x, y, z };
    enqueue(step);
}

extern "C" void LIBDRAW_SYMBOL(tilt)(float degrees, Axis axis) {
    Step step;
    step.type = STEP_TILT;
    step.data.tilt = { degrees, axis };
    enqueue(step);
}

extern "C" void LIBDRAW_SYMBOL(snap)(float x, float y, float z) {
    Step step;
    step.type = STEP_LOOK;
    step.data.look = { x, y, z, 0, 0 };
    enqueue(step);
}

extern "C" void LIBDRAW_SYMBOL(look)(float x, float y, float z, float yaw, float pitch) {
    Step step;
    step.type = STEP_LOOK;
    step.data.look = { x, y, z, yaw, pitch };
    enqueue(step);
}

extern "C" void LIBDRAW_SYMBOL(lookat)(float x1, float y1, float z1, float x2, float y2, float z2) {
    Step step;
    step.type = STEP_LOOK;
    float lookyaw = atan2(x2 - x1, z2 - z1) * 180 / pi;
    float lookdist = sqrt((x2 - x1) * (x2 - x1) + (z2 - z1) * (z2 - z1));
    float lookpitch = atan2(y2 - y1, lookdist) * 180 / pi;
    step.data.look = { x1, y1, z1, lookyaw, lookpitch };
    enqueue(step);
}

extern "C" void LIBDRAW_SYMBOL(paint)(Image img) {
    Image i = currentfbo();
    bindfbo(img);
    flush(rendermodel);
    bindfbo(i);
}

extern "C" void LIBDRAW_SYMBOL(shade)(Image img, Shader shader) {
    Image i = currentfbo();
    Shader s = active_shader();
    bind(shader);
    bindfbo(img);
    flush(rendermodel);
    bind(s);
    bindfbo(i);
}

extern "C" void LIBDRAW_SYMBOL(fog)(Color color, float range) {
    Step step;
    step.type = STEP_FOG;
    step.data.fog = { color, range };
    enqueue(step);
}

extern "C" void LIBDRAW_SYMBOL(nofog)() {
    Step step;
    step.type = STEP_FOG;
    step.data.fog = { WHITE, 0 };
    enqueue(step);
}

extern "C" Model LIBDRAW_SYMBOL(createmodel)() {
    return create_new_model();
}

extern "C" void LIBDRAW_SYMBOL(sketchto)(Model model) {
    Buffer& buf = findbuf(model);
    buf.reset();

    for (const Step& step : steps) {
        if (stateful(step)) drawbuf(buf), buf.reset();
        ::step(buf, step);
    }
    steps.clear();
}

extern "C" Model LIBDRAW_SYMBOL(sketch)() {
    Model m = create_new_model();
    sketchto(m);
    return m;
}

extern "C" void LIBDRAW_SYMBOL(flush)() {
    flush(rendermodel);
}

extern "C" void LIBDRAW_SYMBOL(render)(Model model, Image img) {
    Step step;
    step.type = STEP_RENDER;
    step.data.render = { model, img };
    enqueue(step);
}

void apply_default_uniforms() {
    glUniform1i(find_uniform("width"), width(currentfbo()));
    glUniform1i(find_uniform("height"), height(currentfbo()));
    mode3d ? glUniform3f(find_uniform("light"), lightx, lighty, lightz) : glUniform3f(find_uniform("light"), 0, 0, 1);
    glUniformMatrix4fv(find_uniform("projection"), 1, GL_FALSE, (const GLfloat*)projection);
    glUniformMatrix4fv(find_uniform("view"), 1, GL_FALSE, (const GLfloat*)view);
    glUniformMatrix4fv(find_uniform("model"), 1, GL_FALSE, (const GLfloat*)transform);
    glUniform1i(find_uniform("tex"), 0);
}