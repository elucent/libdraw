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
static float near = 0, far = 0;
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
    float ortho[4][4] = {
        { 2 / w, 0, 0, 0 },
        { 0, 2 / h, 0, 0 },
        { 0, 0, -2 / (far - near), 0 },
        { -1, -1, -(far + near) / (far - near), 1 }
    };
    identity(matrix);
    matmult(matrix, ortho);
}

static void frustum(float matrix[4][4], float w, float h, float fov) {
    float aspect = w / h, top = near * tan(fov * pi / 180), bottom = -top, right = aspect * top, left = -right;
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
    glCullFace(GL_BACK);
    identity(matrix);
    matmult(matrix, frustum);
}

static vector<Step> steps;

void enqueue(const Step& step) {
    steps.push(step);
}

static void drawbuf(Buffer& buf) {
    if (!buf.empty()) {
        buf.draw();
    }
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
            return !mode3d 
                || findimg(step.data.cube.tex.itop).id != texture
                || findimg(step.data.cube.tex.iside).id != texture
                || findimg(step.data.cube.tex.ibottom).id != texture;
        case STEP_SLANT:
            return !mode3d 
                || findimg(step.data.slant.tex.itop).id != texture
                || findimg(step.data.slant.tex.iside).id != texture
                || findimg(step.data.slant.tex.ibottom).id != texture;
        case STEP_PRISM:
            return !mode3d 
                || findimg(step.data.prism.tex.itop).id != texture
                || findimg(step.data.prism.tex.iside).id != texture
                || findimg(step.data.prism.tex.ibottom).id != texture;
        case STEP_CONE:
            return !mode3d 
                || findimg(step.data.cone.tex.iside).id != texture
                || findimg(step.data.cone.tex.itop).id != texture
                || findimg(step.data.cone.tex.ibottom).id != texture;
        case STEP_HEDRON:
            return !mode3d 
                || findimg(step.data.hedron.tex.iside).id != texture
                || findimg(step.data.hedron.tex.itop).id != texture
                || findimg(step.data.hedron.tex.ibottom).id != texture;
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
        case STEP_OPACITY:
        case STEP_UNIFORMI:
        case STEP_UNIFORMF:
        case STEP_UNIFORMV2:
        case STEP_UNIFORMV3:
        case STEP_UNIFORMV4:
        case STEP_SET_LIGHT:
            return true;
        default:
            return false;
    }
}

// static void bindtex(Image i) {
//     if (texture != findimg(i).id) {
//         glBindTexture(GL_TEXTURE_2D, findimg(i).id);
//         texture = findimg(i).id;
//     }
// }

static void bindtex(Buffer& buf, Image i) {
    if (texture != findimg(i).id) {
        drawbuf(buf), buf.reset();
        glBindTexture(GL_TEXTURE_2D, findimg(i).id);
        texture = findimg(i).id;
    }
}

static void plane(Buffer& buf, 
    float x, float y, float z, float w, float h,  
    float hx, float hy, float hz, 
    float vx, float vy, float vz,
    Image i) {
    bindtex(buf, i);
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
    float u_auto = frem(u, iw) / iw, v_auto = frem(ih - v, ih) / ih, w_auto = w / iw, h_auto = h / ih;
    buf.uv(u_auto, v_auto);
    buf.uv(u_auto + w_auto, v_auto);
    buf.uv(u_auto + w_auto, v_auto + h_auto);
    buf.uv(u_auto + w_auto, v_auto + h_auto);
    buf.uv(u_auto, v_auto + h_auto);
    buf.uv(u_auto, v_auto);
}

static void stretchuv(Buffer& buf) {
    buf.uv(1, 0);
    buf.uv(0, 0);
    buf.uv(0, 1);
    buf.uv(0, 1);
    buf.uv(1, 1);
    buf.uv(1, 0);
}

static void polygon(Buffer& buf, float x, float y, float r, float n) {
    bindtex(buf, BLANK);
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

struct TexProps {
    float u, v, iw, ih, uw, vh;

    void use(Image i) {
        ImageMeta* meta = &findimg(i);
        while (meta->parent > 0) meta = &findimg(meta->parent);
        u = float(findimg(i).x) / meta->w, v = float(findimg(i).y) / meta->h;
        iw = float(findimg(i).w), ih = float(findimg(i).h);
        uw = float(findimg(i).w) / meta->w, vh = float(findimg(i).h) / meta->h;
    }

    void tspr(Buffer& buf) {
        for (int i = 0; i < 3; i ++) buf.spr(u, v, uw, vh);
    }

    void qspr(Buffer& buf) {
        for (int i = 0; i < 6; i ++) buf.spr(u, v, uw, vh);
    }
};

static void cube(Buffer& buf, float x, float y, float z, float w, float h, float l, Texture tex) {
    bindtex(buf, tex.iside);
    float dx = w / 2, dy = h / 2, dz = l / 2;
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

    TexProps tp;

    x -= dx;
    y -= dy;
    z -= dz;

    switch (tex.type) {
        default:
        case LIBDRAW_CONST(AUTO_CUBE):
        case LIBDRAW_CONST(AUTO_PILLAR):
        case LIBDRAW_CONST(AUTO_SIDED):
            tp.use(tex.iside), bindtex(buf, tex.iside), tp.qspr(buf); tp.qspr(buf);
            autouv(buf, z, y, l, h, tp.iw, tp.ih); // X axis
            autouv(buf, z, y, -l, h, -tp.iw, tp.ih);
            tp.use(tex.ibottom), bindtex(buf, tex.ibottom), tp.qspr(buf);
            autouv(buf, z, x, -l, w, -tp.iw, tp.ih); // Y axis
            tp.use(tex.itop), bindtex(buf, tex.itop), tp.qspr(buf);
            autouv(buf, z, x, l, w, tp.iw, tp.ih);
            tp.use(tex.iside), bindtex(buf, tex.iside), tp.qspr(buf); tp.qspr(buf);
            autouv(buf, x, y, -w, h, -tp.iw, tp.ih); // Z axis
            autouv(buf, x, y, w, h, tp.iw, tp.ih);
            break;
        case LIBDRAW_CONST(STRETCH_CUBE):
        case LIBDRAW_CONST(STRETCH_PILLAR):
        case LIBDRAW_CONST(STRETCH_SIDED):
            tp.use(tex.iside), bindtex(buf, tex.iside), tp.qspr(buf); tp.qspr(buf);
            for (int i = 0; i < 2; i ++) stretchuv(buf);
            tp.use(tex.ibottom), bindtex(buf, tex.ibottom), tp.qspr(buf);
            stretchuv(buf);
            tp.use(tex.itop), bindtex(buf, tex.itop), tp.qspr(buf);
            stretchuv(buf);
            tp.use(tex.iside), bindtex(buf, tex.iside), tp.qspr(buf); tp.qspr(buf);
            for (int i = 0; i < 2; i ++) stretchuv(buf);
            break;
    }
}

static void prism(Buffer& buf, float x, float y, float z, float w, float h, float l, int n, Axis axis, Texture tex) {
    float dx = w / 2, dy = h / 2, dz = l / 2;
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

    TexProps tp;

    // front
    bindtex(buf, tex.itop);
    tp.use(tex.itop);
    
    for (int i = 0; i < n; i ++) {
        float a1 = 2 * pi * i / n - 0.5 * pi, a2 = 2 * pi * (i + 1) / n - 0.5 * pi;
        for (int j = 0; j < 3; j ++) {
            buf.norm(ln[0], ln[1], ln[2]); 
            buf.col(red, green, blue, alpha); 
            buf.spr(tp.u, tp.v, tp.uw * (endw / tp.iw), tp.vh * (endh / tp.ih));
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
    bindtex(buf, tex.ibottom);
    tp.use(tex.ibottom);

    for (int i = 0; i < n; i ++) {
        float a1 = 2 * pi * i / n - 0.5 * pi, a2 = 2 * pi * (i + 1) / n - 0.5 * pi;
        for (int j = 0; j < 3; j ++) {
            buf.norm(-ln[0], -ln[1], -ln[2]); 
            buf.col(red, green, blue, alpha); 
            buf.spr(tp.u + (2 * sidew / 3 / tp.iw) * tp.uw, tp.v + (endh / tp.ih + sideh / tp.ih) * tp.vh, tp.uw * (endw / tp.iw), tp.vh * (endh / tp.ih));
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
    bindtex(buf, tex.iside);
    tp.use(tex.iside);

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
            buf.spr(tp.u, tp.v + (endh / tp.ih) * tp.vh, tp.uw * (sidew / tp.iw), tp.vh * (sideh / tp.ih));
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
    Axis axis = (Axis)(dir / 2);
    float dx = w / 2, dy = h / 2, dz = l / 2;
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

    TexProps tp;

    // front
    bindtex(buf, tex.itop);
    tp.use(tex.itop);
    if (flipped) for (int i = 0; i < n; i ++) {
        float a1 = 2 * pi * i / n - 0.5 * pi, a2 = 2 * pi * (i + 1) / n - 0.5 * pi;
        for (int j = 0; j < 3; j ++) {
            buf.norm(ln[0], ln[1], ln[2]); 
            buf.col(red, green, blue, alpha); 
            buf.spr(tp.u, tp.v, tp.uw * (endw / tp.iw), tp.vh * (endh / tp.ih));
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
    bindtex(buf, tex.ibottom);
    tp.use(tex.ibottom);
    if (!flipped) for (int i = 0; i < n; i ++) {
        float a1 = 2 * pi * i / n - 0.5 * pi, a2 = 2 * pi * (i + 1) / n - 0.5 * pi;
        for (int j = 0; j < 3; j ++) {
            buf.norm(-ln[0], -ln[1], -ln[2]); 
            buf.col(red, green, blue, alpha); 
            buf.spr(tp.u, tp.v, tp.uw * (endw / tp.iw), tp.vh * (endh / tp.ih));
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
    bindtex(buf, tex.iside);
    tp.use(tex.iside);
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
            buf.spr(tp.u, tp.v + (endh / tp.ih) * tp.vh, tp.uw * (sidew / tp.iw), tp.vh * (sideh / tp.ih));
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
    float dx = w / 2, dy = h / 2, dz = l / 2;
    float ox = int(orig) % 3 - 1, oy = int(orig) % 9 / 3 - 1, oz = int(orig) / 9 - 1;
    x -= w * ox / 2; y -= h * oy / 2; z -= l * oz / 2;
    
    float* mercator = new float[n + 1];
    for (int i = 1; i < n; i ++) {
        float phi = (float(i) / n - 0.5f) * pi;
        mercator[i] = fmin(10.0f, log(abs(1 / cos(phi) + tan(phi))));
    }
    mercator[0] = -10.0f;
    mercator[n] = 10.0f;  
    for (int i = 0; i <= n; i ++) mercator[i] += 10.0f; 
    float msum = 0;
    for (int i = 0; i <= n; i ++) {
        msum += mercator[i];
        if (i > 1) mercator[i] += mercator[i - 1];
    }
    for (int i = 0; i <= n; i ++) mercator[i] /= msum;

    TexProps tp;
    bindtex(buf, tex.iside);
    tp.use(tex.iside);

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
                buf.spr(tp.u, tp.v, tp.uw, tp.vh);
            }
            buf.pos(x + dx * cp1 * sy1, y + dy * sp1, z + dz * cp1 * cy1);
            buf.pos(x + dx * cp1 * sy2, y + dy * sp1, z + dz * cp1 * cy2);
            buf.pos(x + dx * cp2 * sy2, y + dy * sp2, z + dz * cp2 * cy2);
            buf.pos(x + dx * cp2 * sy2, y + dy * sp2, z + dz * cp2 * cy2);
            buf.pos(x + dx * cp2 * sy1, y + dy * sp2, z + dz * cp2 * cy1);
            buf.pos(x + dx * cp1 * sy1, y + dy * sp1, z + dz * cp1 * cy1);
            
            // mercator scaling

            buf.uv(float(i) / m, mercator[j]);
            buf.uv(float(i + 1) / m, mercator[j]);
            buf.uv(float(i + 1) / m, mercator[j + 1]);
            buf.uv(float(i + 1) / m, mercator[j + 1]);
            buf.uv(float(i) / m, mercator[j + 1]);
            buf.uv(float(i) / m, mercator[j]);
        }
    }

    delete[] mercator;
}

static void text(Buffer& buf, float x, float y, const char* str, float width) {
    bindtex(buf, currentfont);
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
    float u_auto = frem(u, iw) / iw, v_auto = frem(ih - v, ih) / ih, w_auto = w / iw, h_auto = h / ih;
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
    float dx = w / 2, dy = h / 2, dz = l / 2;
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

    TexProps tp;

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

            x -= dx;
            y -= dy;
            z -= dz;
            switch (tex.type) {
                case LIBDRAW_CONST(AUTO_CUBE):
                case LIBDRAW_CONST(AUTO_PILLAR):
                case LIBDRAW_CONST(AUTO_SIDED):
                    bindtex(buf, tex.iside), tp.use(tex.iside), tp.qspr(buf), tp.qspr(buf);
                    autouv(buf, z, y, l, h, tp.iw, tp.ih);
                    if (l * h > w * l) autouv(buf, z, y, -l, h, tp.iw, tp.ih);
                    else autouv(buf, z, x, -l, w, tp.iw, tp.ih);
                    bindtex(buf, tex.itop), tp.use(tex.itop), tp.qspr(buf);
                    autouv(buf, z, x, l, w, tp.iw, tp.ih);
                    bindtex(buf, tex.iside), tp.use(tex.iside), tp.tspr(buf), tp.tspr(buf);
                    autouvtri(buf, x, y, -w, h, tp.iw, tp.ih, true, false);
                    autouvtri(buf, x, y, w, h, tp.iw, tp.ih, false, false);
                    break;
                case LIBDRAW_CONST(STRETCH_CUBE):
                case LIBDRAW_CONST(STRETCH_PILLAR):
                case LIBDRAW_CONST(STRETCH_SIDED):
                    bindtex(buf, tex.iside), tp.use(tex.iside), tp.qspr(buf), tp.qspr(buf);
                    stretchuv(buf);
                    stretchuv(buf);
                    bindtex(buf, tex.itop), tp.use(tex.itop), tp.qspr(buf);
                    stretchuv(buf);
                    bindtex(buf, tex.iside), tp.use(tex.iside), tp.tspr(buf), tp.tspr(buf);
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

            x -= dx;
            y -= dy;
            z -= dz;
            switch (tex.type) {
                case LIBDRAW_CONST(AUTO_CUBE):
                case LIBDRAW_CONST(AUTO_PILLAR):
                case LIBDRAW_CONST(AUTO_SIDED):
                    bindtex(buf, tex.iside), tp.use(tex.iside), tp.tspr(buf), tp.tspr(buf);
                    autouvtri(buf, z, y, l, h, tp.iw, tp.ih, true, false);
                    autouvtri(buf, z, y, -l, h, tp.iw, tp.ih, false, false);
                    bindtex(buf, tex.itop), tp.use(tex.itop), tp.qspr(buf);
                    autouv(buf, z, x, l, w, tp.iw, tp.ih);
                    bindtex(buf, tex.iside), tp.use(tex.iside), tp.qspr(buf), tp.qspr(buf);
                    if (w * h > w * l) autouv(buf, x, y, -w, h, tp.iw, tp.ih);
                    else autouv(buf, z, x, -l, w, tp.iw, tp.ih);
                    autouv(buf, x, y, w, h, tp.iw, tp.ih);
                    break;
                case LIBDRAW_CONST(STRETCH_CUBE):
                case LIBDRAW_CONST(STRETCH_PILLAR):
                case LIBDRAW_CONST(STRETCH_SIDED):
                    bindtex(buf, tex.iside), tp.use(tex.iside), tp.tspr(buf), tp.tspr(buf);
                    stretchuvtri(buf, true, false);
                    stretchuvtri(buf, false, false);
                    bindtex(buf, tex.itop), tp.use(tex.itop), tp.qspr(buf);
                    stretchuv(buf);
                    bindtex(buf, tex.iside), tp.use(tex.iside), tp.qspr(buf), tp.qspr(buf);
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

            x -= dx;
            y -= dy;
            z -= dz;
            switch (tex.type) {
                case LIBDRAW_CONST(AUTO_CUBE):
                case LIBDRAW_CONST(AUTO_PILLAR):
                case LIBDRAW_CONST(AUTO_SIDED):
                    bindtex(buf, tex.iside), tp.use(tex.iside), tp.qspr(buf), tp.qspr(buf);
                    autouv(buf, z, y, l, h, tp.iw, tp.ih);
                    if (l * h > w * l) autouv(buf, z, y, -l, h, tp.iw, tp.ih);
                    else autouv(buf, z, x, -l, w, tp.iw, tp.ih);
                    bindtex(buf, tex.itop), tp.use(tex.itop), tp.qspr(buf);
                    autouv(buf, z, x, l, w, tp.iw, tp.ih);
                    bindtex(buf, tex.iside), tp.use(tex.iside), tp.tspr(buf), tp.tspr(buf);
                    autouvtri(buf, x, y, -w, h, tp.iw, tp.ih, false, false);
                    autouvtri(buf, x, y, w, h, tp.iw, tp.ih, true, false);
                    break;
                case LIBDRAW_CONST(STRETCH_CUBE):
                case LIBDRAW_CONST(STRETCH_PILLAR):
                case LIBDRAW_CONST(STRETCH_SIDED):
                    bindtex(buf, tex.iside), tp.use(tex.iside), tp.qspr(buf), tp.qspr(buf);
                    stretchuv(buf);
                    stretchuv(buf);
                    bindtex(buf, tex.itop), tp.use(tex.itop), tp.qspr(buf);
                    stretchuv(buf);
                    bindtex(buf, tex.iside), tp.use(tex.iside), tp.tspr(buf), tp.tspr(buf);
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

            x -= dx;
            y -= dy;
            z -= dz;
            switch (tex.type) {
                case LIBDRAW_CONST(AUTO_CUBE):
                case LIBDRAW_CONST(AUTO_PILLAR):
                case LIBDRAW_CONST(AUTO_SIDED):
                    bindtex(buf, tex.iside), tp.use(tex.iside), tp.tspr(buf), tp.tspr(buf);
                    autouvtri(buf, z, y, l, h, tp.iw, tp.ih, false, false);
                    autouvtri(buf, z, y, -l, h, tp.iw, tp.ih, true, false);
                    bindtex(buf, tex.itop), tp.use(tex.itop), tp.qspr(buf);
                    autouv(buf, z, x, l, w, tp.iw, tp.ih);
                    bindtex(buf, tex.iside), tp.use(tex.iside), tp.qspr(buf), tp.qspr(buf);
                    if (w * h > w * l) autouv(buf, x, y, -w, h, tp.iw, tp.ih);
                    else autouv(buf, z, x, -l, w, tp.iw, tp.ih);
                    autouv(buf, x, y, w, h, tp.iw, tp.ih);
                    break;
                case LIBDRAW_CONST(STRETCH_CUBE):
                case LIBDRAW_CONST(STRETCH_PILLAR):
                case LIBDRAW_CONST(STRETCH_SIDED):
                    bindtex(buf, tex.iside), tp.use(tex.iside), tp.tspr(buf), tp.tspr(buf);
                    stretchuvtri(buf, false, false);
                    stretchuvtri(buf, true, false);
                    bindtex(buf, tex.itop), tp.use(tex.itop), tp.qspr(buf);
                    stretchuv(buf);
                    bindtex(buf, tex.iside), tp.use(tex.iside), tp.qspr(buf), tp.qspr(buf);
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

            x -= dx;
            y -= dy;
            z -= dz;
            switch (tex.type) {
                case LIBDRAW_CONST(AUTO_CUBE):
                case LIBDRAW_CONST(AUTO_PILLAR):
                case LIBDRAW_CONST(AUTO_SIDED):
                    bindtex(buf, tex.iside), tp.use(tex.iside), tp.qspr(buf), tp.qspr(buf);
                    autouv(buf, z, y, l, h, tp.iw, tp.ih); // negative x
                    if (!cullX) autouv(buf, z, y, -l, h, tp.iw, tp.ih); // positive x
                    bindtex(buf, tex.ibottom), tp.use(tex.ibottom), tp.tspr(buf);
                    autouvtri(buf, z, x, -l, w, tp.iw, tp.ih, true, true); // negative y
                    bindtex(buf, tex.itop), tp.use(tex.itop), tp.tspr(buf);
                    autouvtri(buf, z, x, l, w, tp.iw, tp.ih, false, true); // positive y
                    bindtex(buf, tex.iside), tp.use(tex.iside), tp.qspr(buf);
                    autouv(buf, x, y, -w, h, tp.iw, tp.ih); // negative z
                    if (cullX) autouv(buf, x, y, w, h, tp.iw, tp.ih); // positive z
                    break;
                case LIBDRAW_CONST(STRETCH_CUBE):
                case LIBDRAW_CONST(STRETCH_PILLAR):
                case LIBDRAW_CONST(STRETCH_SIDED):
                    bindtex(buf, tex.iside), tp.use(tex.iside), tp.qspr(buf);
                    stretchuv(buf);
                    if (!cullX) tp.qspr(buf), stretchuv(buf);
                    bindtex(buf, tex.ibottom), tp.use(tex.ibottom), tp.tspr(buf);
                    stretchuvtri(buf, true, true);
                    bindtex(buf, tex.itop), tp.use(tex.itop), tp.tspr(buf);
                    stretchuvtri(buf, false, true);
                    bindtex(buf, tex.iside), tp.use(tex.iside), tp.qspr(buf);
                    stretchuv(buf);
                    if (cullX) tp.qspr(buf), stretchuv(buf);
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

            x -= dx;
            y -= dy;
            z -= dz;
            switch (tex.type) {
                case LIBDRAW_CONST(AUTO_CUBE):
                case LIBDRAW_CONST(AUTO_PILLAR):
                case LIBDRAW_CONST(AUTO_SIDED):
                    bindtex(buf, tex.iside), tp.use(tex.iside), tp.qspr(buf);
                    if (!cullX) tp.qspr(buf), autouv(buf, z, y, l, h, tp.iw, tp.ih); // negative x
                    autouv(buf, z, y, -l, h, tp.iw, tp.ih); // positive x
                    bindtex(buf, tex.ibottom), tp.use(tex.ibottom), tp.tspr(buf);
                    autouvtri(buf, z, x, -l, w, tp.iw, tp.ih, true, false); // negative y
                    bindtex(buf, tex.itop), tp.use(tex.itop), tp.tspr(buf);
                    autouvtri(buf, z, x, l, w, tp.iw, tp.ih, false, false); // positive y
                    bindtex(buf, tex.iside), tp.use(tex.iside), tp.qspr(buf);
                    autouv(buf, x, y, -w, h, tp.iw, tp.ih); // negative z
                    if (cullX) tp.qspr(buf), autouv(buf, x, y, w, h, tp.iw, tp.ih); // positive z
                    break;
                case LIBDRAW_CONST(STRETCH_CUBE):
                case LIBDRAW_CONST(STRETCH_PILLAR):
                case LIBDRAW_CONST(STRETCH_SIDED):
                    bindtex(buf, tex.iside), tp.use(tex.iside), tp.qspr(buf);
                    if (!cullX) tp.qspr(buf), stretchuv(buf);
                    stretchuv(buf);
                    bindtex(buf, tex.ibottom), tp.use(tex.ibottom), tp.tspr(buf);
                    stretchuvtri(buf, true, false);
                    bindtex(buf, tex.itop), tp.use(tex.itop), tp.tspr(buf);
                    stretchuvtri(buf, false, false);
                    bindtex(buf, tex.iside), tp.use(tex.iside), tp.qspr(buf);
                    stretchuv(buf);
                    if (cullX) tp.qspr(buf), stretchuv(buf);
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

            x -= dx;
            y -= dy;
            z -= dz;
            switch (tex.type) {
                case LIBDRAW_CONST(AUTO_CUBE):
                case LIBDRAW_CONST(AUTO_PILLAR):
                case LIBDRAW_CONST(AUTO_SIDED):
                    bindtex(buf, tex.iside), tp.use(tex.iside), tp.qspr(buf);
                    if (!cullX) tp.qspr(buf), autouv(buf, z, y, l, h, tp.iw, tp.ih); // negative x
                    autouv(buf, z, y, -l, h, tp.iw, tp.ih); // positive x
                    bindtex(buf, tex.ibottom), tp.use(tex.ibottom), tp.tspr(buf);
                    autouvtri(buf, z, x, -l, w, tp.iw, tp.ih, false, false); // negative y
                    bindtex(buf, tex.itop), tp.use(tex.itop), tp.tspr(buf);
                    autouvtri(buf, z, x, l, w, tp.iw, tp.ih, true, false); // positive y
                    bindtex(buf, tex.iside), tp.use(tex.iside), tp.qspr(buf);
                    if (cullX) tp.qspr(buf), autouv(buf, x, y, -w, h, tp.iw, tp.ih); // negative z
                    autouv(buf, x, y, w, h, tp.iw, tp.ih); // positive z
                    break;
                case LIBDRAW_CONST(STRETCH_CUBE):
                case LIBDRAW_CONST(STRETCH_PILLAR):
                case LIBDRAW_CONST(STRETCH_SIDED):
                    bindtex(buf, tex.iside), tp.use(tex.iside), tp.qspr(buf);
                    if (!cullX) tp.qspr(buf), stretchuv(buf);
                    stretchuv(buf);
                    bindtex(buf, tex.ibottom), tp.use(tex.ibottom), tp.tspr(buf);
                    stretchuvtri(buf, false, false);
                    bindtex(buf, tex.itop), tp.use(tex.itop), tp.tspr(buf);
                    stretchuvtri(buf, true, false);
                    bindtex(buf, tex.iside), tp.use(tex.iside), tp.qspr(buf);
                    if (cullX) tp.qspr(buf), stretchuv(buf);
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

            x -= dx;
            y -= dy;
            z -= dz;
            switch (tex.type) {
                case LIBDRAW_CONST(AUTO_CUBE):
                case LIBDRAW_CONST(AUTO_PILLAR):
                case LIBDRAW_CONST(AUTO_SIDED):
                    bindtex(buf, tex.iside), tp.use(tex.iside), tp.qspr(buf);
                    autouv(buf, z, y, l, h, tp.iw, tp.ih); // negative x
                    if (!cullX) tp.qspr(buf), autouv(buf, z, y, -l, h, tp.iw, tp.ih); // positive x
                    bindtex(buf, tex.ibottom), tp.use(tex.ibottom), tp.tspr(buf);
                    autouvtri(buf, z, x, -l, w, tp.iw, tp.ih, false, true); // negative y
                    bindtex(buf, tex.itop), tp.use(tex.itop), tp.tspr(buf);
                    autouvtri(buf, z, x, l, w, tp.iw, tp.ih, true, true); // positive y
                    bindtex(buf, tex.iside), tp.use(tex.iside), tp.qspr(buf);
                    if (cullX) tp.qspr(buf), autouv(buf, x, y, -w, h, tp.iw, tp.ih); // negative z
                    autouv(buf, x, y, w, h, tp.iw, tp.ih); // positive z
                    break;
                case LIBDRAW_CONST(STRETCH_CUBE):
                case LIBDRAW_CONST(STRETCH_PILLAR):
                case LIBDRAW_CONST(STRETCH_SIDED):
                    bindtex(buf, tex.iside), tp.use(tex.iside), tp.qspr(buf);
                    stretchuv(buf);
                    if (!cullX) tp.qspr(buf), stretchuv(buf);
                    bindtex(buf, tex.ibottom), tp.use(tex.ibottom), tp.tspr(buf);
                    stretchuvtri(buf, false, true);
                    bindtex(buf, tex.itop), tp.use(tex.itop), tp.tspr(buf);
                    stretchuvtri(buf, true, true);
                    bindtex(buf, tex.iside), tp.use(tex.iside), tp.qspr(buf);
                    if (cullX) tp.qspr(buf), stretchuv(buf);
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

            x -= dx;
            y -= dy;
            z -= dz;
            switch (tex.type) {
                case LIBDRAW_CONST(AUTO_CUBE):
                case LIBDRAW_CONST(AUTO_PILLAR):
                case LIBDRAW_CONST(AUTO_SIDED):
                    bindtex(buf, tex.iside), tp.use(tex.iside), tp.qspr(buf), tp.qspr(buf);
                    autouv(buf, z, y, l, h, tp.iw, tp.ih);
                    if (l * h > w * h) autouv(buf, z, y, -l, h, tp.iw, tp.ih);
                    else autouv(buf, z, x, -l, w, tp.iw, tp.ih);
                    bindtex(buf, tex.ibottom), tp.use(tex.ibottom), tp.qspr(buf);
                    autouv(buf, z, x, -l, w, tp.iw, tp.ih);
                    bindtex(buf, tex.iside), tp.use(tex.iside), tp.tspr(buf), tp.tspr(buf);
                    autouvtri(buf, x, y, -w, h, tp.iw, tp.ih, true, true);
                    autouvtri(buf, x, y, w, h, tp.iw, tp.ih, false, true);
                    break;
                case LIBDRAW_CONST(STRETCH_CUBE):
                case LIBDRAW_CONST(STRETCH_PILLAR):
                case LIBDRAW_CONST(STRETCH_SIDED):
                    bindtex(buf, tex.iside), tp.use(tex.iside), tp.qspr(buf), tp.qspr(buf);
                    stretchuv(buf);
                    stretchuv(buf);
                    bindtex(buf, tex.ibottom), tp.use(tex.ibottom), tp.qspr(buf);
                    stretchuv(buf);
                    bindtex(buf, tex.iside), tp.use(tex.iside), tp.tspr(buf), tp.tspr(buf);
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

            x -= dx;
            y -= dy;
            z -= dz;
            switch (tex.type) {
                case LIBDRAW_CONST(AUTO_CUBE):
                case LIBDRAW_CONST(AUTO_PILLAR):
                case LIBDRAW_CONST(AUTO_SIDED):
                    bindtex(buf, tex.iside), tp.use(tex.iside), tp.tspr(buf), tp.tspr(buf);
                    autouvtri(buf, z, y, l, h, tp.iw, tp.ih, true, true);
                    autouvtri(buf, z, y, -l, h, tp.iw, tp.ih, false, true);
                    bindtex(buf, tex.ibottom), tp.use(tex.ibottom), tp.qspr(buf);
                    autouv(buf, z, x, -l, w, tp.iw, tp.ih);
                    bindtex(buf, tex.iside), tp.use(tex.iside), tp.qspr(buf), tp.qspr(buf);
                    if (w * h > w * l) autouv(buf, x, y, -w, h, tp.iw, tp.ih);
                    else autouv(buf, z, x, -l, w, tp.iw, tp.ih);
                    autouv(buf, x, y, w, h, tp.iw, tp.ih);
                    break;
                case LIBDRAW_CONST(STRETCH_CUBE):
                case LIBDRAW_CONST(STRETCH_PILLAR):
                case LIBDRAW_CONST(STRETCH_SIDED):
                    bindtex(buf, tex.iside), tp.use(tex.iside), tp.tspr(buf), tp.tspr(buf);
                    stretchuvtri(buf, true, true);
                    stretchuvtri(buf, false, true);
                    bindtex(buf, tex.ibottom), tp.use(tex.ibottom), tp.qspr(buf);
                    stretchuv(buf);
                    bindtex(buf, tex.iside), tp.use(tex.iside), tp.qspr(buf), tp.qspr(buf);
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

            x -= dx;
            y -= dy;
            z -= dz;
            switch (tex.type) {
                case LIBDRAW_CONST(AUTO_CUBE):
                case LIBDRAW_CONST(AUTO_PILLAR):
                case LIBDRAW_CONST(AUTO_SIDED):
                    bindtex(buf, tex.iside), tp.use(tex.iside), tp.qspr(buf), tp.qspr(buf);
                    autouv(buf, z, y, l, h, tp.iw, tp.ih);
                    if (l * h > w * h) autouv(buf, z, y, -l, h, tp.iw, tp.ih);
                    else autouv(buf, z, x, -l, w, tp.iw, tp.ih);
                    bindtex(buf, tex.ibottom), tp.use(tex.ibottom), tp.qspr(buf);
                    autouv(buf, z, x, -l, w, tp.iw, tp.ih);
                    bindtex(buf, tex.iside), tp.use(tex.iside), tp.tspr(buf), tp.tspr(buf);
                    autouvtri(buf, x, y, -w, h, tp.iw, tp.ih, false, true);
                    autouvtri(buf, x, y, w, h, tp.iw, tp.ih, true, true);
                    break;
                case LIBDRAW_CONST(STRETCH_CUBE):
                case LIBDRAW_CONST(STRETCH_PILLAR):
                case LIBDRAW_CONST(STRETCH_SIDED):
                    bindtex(buf, tex.iside), tp.use(tex.iside), tp.qspr(buf), tp.qspr(buf);
                    stretchuv(buf);
                    stretchuv(buf);
                    bindtex(buf, tex.ibottom), tp.use(tex.ibottom), tp.qspr(buf);
                    stretchuv(buf);
                    bindtex(buf, tex.iside), tp.use(tex.iside), tp.tspr(buf), tp.tspr(buf);
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

            x -= dx;
            y -= dy;
            z -= dz;
            switch (tex.type) {
                case LIBDRAW_CONST(AUTO_CUBE):
                case LIBDRAW_CONST(AUTO_PILLAR):
                case LIBDRAW_CONST(AUTO_SIDED):
                    bindtex(buf, tex.iside), tp.use(tex.iside), tp.tspr(buf), tp.tspr(buf);
                    autouvtri(buf, z, y, l, h, tp.iw, tp.ih, false, true);
                    autouvtri(buf, z, y, -l, h, tp.iw, tp.ih, true, true);
                    bindtex(buf, tex.ibottom), tp.use(tex.ibottom), tp.qspr(buf);
                    autouv(buf, z, x, -l, w, tp.iw, tp.ih);
                    bindtex(buf, tex.iside), tp.use(tex.iside), tp.qspr(buf), tp.qspr(buf);
                    if (w * h > w * l) autouv(buf, x, y, -w, h, tp.iw, tp.ih);
                    else autouv(buf, z, x, -l, w, tp.iw, tp.ih);
                    autouv(buf, x, y, w, h, tp.iw, tp.ih);
                    break;
                case LIBDRAW_CONST(STRETCH_CUBE):
                case LIBDRAW_CONST(STRETCH_PILLAR):
                case LIBDRAW_CONST(STRETCH_SIDED):
                    bindtex(buf, tex.iside), tp.use(tex.iside), tp.tspr(buf), tp.tspr(buf);
                    stretchuvtri(buf, false, true);
                    stretchuvtri(buf, true, true);
                    bindtex(buf, tex.ibottom), tp.use(tex.ibottom), tp.qspr(buf);
                    stretchuv(buf);
                    bindtex(buf, tex.iside), tp.use(tex.iside), tp.qspr(buf), tp.qspr(buf);
                    stretchuv(buf);
                    stretchuv(buf);
                    break;
            }
            break;
        }
    }

    for (int i = 0; i < 24; i ++) buf.col(red, green, blue, alpha);
}

void ensure2d() {
    if (mode3d) {
        mode3d = false;
        glUniform3f(find_uniform("light"), 0, 0, 1);
        glDepthMask(GL_FALSE);
        glCullFace(GL_FRONT);
    }
}

void ensure3d() {
    if (!mode3d) {
        mode3d = true;
        glUniform3f(find_uniform("light"), lightx, lighty, lightz);
        glDepthMask(GL_TRUE);
        glCullFace(GL_BACK);
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
            bindtex(buf, c.tex.iside);
            return cube(buf, c.x, c.y, c.z, c.w, c.h, c.l, c.tex);
        }
        case STEP_BOARD: {
            ensure3d();
            auto& b = step.data.board;
            bindtex(buf, b.img);
            return plane(buf, b.x, b.y, b.z, b.w, b.h, boardh[0], boardh[1], boardh[2], boardv[0], boardv[1], boardv[2], b.img);
        }
        case STEP_SLANT: {
            ensure3d();
            auto& c = step.data.slant;
            bindtex(buf, c.tex.iside);
            return slant(buf, c.x, c.y, c.z, c.w, c.h, c.l, c.edge, c.tex);
        }
        case STEP_PRISM: {
            ensure3d();
            auto& p = step.data.prism;
            bindtex(buf, p.tex.iside);
            return prism(buf, p.x, p.y, p.z, p.w, p.h, p.l, p.n, p.axis, p.tex);
        }
        case STEP_CONE: {
            ensure3d();
            auto& c = step.data.cone;
            bindtex(buf, c.tex.iside);
            return pyramid(buf, c.x, c.y, c.z, c.w, c.h, c.l, c.n, c.dir, c.tex);
        }
        case STEP_HEDRON: {
            ensure3d();
            auto& h = step.data.hedron;
            bindtex(buf, h.tex.iside);
            return hedron(buf, h.x, h.y, h.z, h.w, h.h, h.l, h.m, h.n, h.tex);
        }
        case STEP_ORTHO: {
            identity(projection);
            near = -1000, far = 1000;
            ortho(projection, step.data.ortho.w, step.data.ortho.h);
            glUniform1f(find_uniform("near"), near);
            glUniform1f(find_uniform("far"), far);
            glUniformMatrix4fv(find_uniform("projection"), 1, GL_FALSE, (const GLfloat*)projection);
            return;
        }
        case STEP_FRUSTUM: {
            identity(projection);
            near = 0.125, far = 1000;
            frustum(projection, step.data.frustum.w, step.data.frustum.h, step.data.frustum.fov);
            glUniform1f(find_uniform("near"), near);
            glUniform1f(find_uniform("far"), far);
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
            bindtex(buf, step.data.render.img);
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
        case STEP_OPACITY: {
            if (step.data.opacity.opacity == LIBDRAW_CONST(NORMAL_OPACITY)) 
                glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            else if (step.data.opacity.opacity == LIBDRAW_CONST(MULTIPLICATIVE_OPACITY)) 
                glBlendFuncSeparate(GL_ZERO, GL_SRC_COLOR, GL_ZERO, GL_DST_ALPHA);
            else if (step.data.opacity.opacity == LIBDRAW_CONST(ADDITIVE_OPACITY)) glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            return;
        }
        case STEP_UNIFORMI: {
            Shader sh = active_shader();
            if (sh != step.data.uniformi.shader) glUseProgram(find_shader(step.data.uniformi.shader));
            glUniform1i(find_uniform(step.data.uniformi.shader, step.data.uniformi.name), step.data.uniformi.i);
            delete[] step.data.uniformi.name;
            if (sh != step.data.uniformi.shader) glUseProgram(find_shader(sh));
            return;
        }
        case STEP_UNIFORMF: {
            Shader sh = active_shader();
            if (sh != step.data.uniformf.shader) glUseProgram(find_shader(step.data.uniformf.shader));
            glUniform1f(find_uniform(step.data.uniformf.shader, step.data.uniformf.name), step.data.uniformf.f);
            delete[] step.data.uniformf.name;
            if (sh != step.data.uniformf.shader) glUseProgram(find_shader(sh));
            return;
        }
        case STEP_UNIFORMV2: {
            Shader sh = active_shader();
            if (sh != step.data.uniformv2.shader) glUseProgram(find_shader(step.data.uniformv2.shader));
            glUniform2f(find_uniform(step.data.uniformv2.shader, step.data.uniformv2.name), step.data.uniformv2.x, step.data.uniformv2.y);
            delete[] step.data.uniformv2.name;
            if (sh != step.data.uniformv2.shader) glUseProgram(find_shader(sh));
            return;
        }
        case STEP_UNIFORMV3: {
            Shader sh = active_shader();
            if (sh != step.data.uniformv3.shader) glUseProgram(find_shader(step.data.uniformv3.shader));
            glUniform3f(find_uniform(step.data.uniformv3.shader, step.data.uniformv3.name), step.data.uniformv3.x, step.data.uniformv3.y, step.data.uniformv3.z);
            delete[] step.data.uniformv3.name;
            if (sh != step.data.uniformv3.shader) glUseProgram(find_shader(sh));
            return;
        }
        case STEP_UNIFORMV4: {
            Shader sh = active_shader();
            if (sh != step.data.uniformv4.shader) glUseProgram(find_shader(step.data.uniformv4.shader));
            glUniform4f(find_uniform(step.data.uniformv4.shader, step.data.uniformv4.name), step.data.uniformv4.x, step.data.uniformv4.y, step.data.uniformv4.z, step.data.uniformv4.w);
            delete[] step.data.uniformv4.name;
            if (sh != step.data.uniformv4.shader) glUseProgram(find_shader(sh));
            return;
        }
        case STEP_UNIFORMTEX: {
            Shader sh = active_shader();
            GLuint texid = GL_TEXTURE0 + step.data.uniformtex.id;
            if (texid == GL_TEXTURE0) {
                fprintf(stderr, "Could not bind uniform %s: Libdraw forbids use of id 0 in texture uniforms.\n", step.data.uniformtex.name);
                exit(1);
            }
            if (texid > GL_TEXTURE31) {
                fprintf(stderr, "Texture uniform %s with id %d exceeds maximum texture id %d.\n", step.data.uniformtex.name, step.data.uniformtex.id, 31);
                exit(1);
            }
            if (sh != step.data.uniformtex.shader) glUseProgram(find_shader(step.data.uniformtex.shader));
            glEnable(texid);
            glActiveTexture(texid);
            glBindTexture(GL_TEXTURE_2D, findimg(step.data.uniformtex.i).id);
            glActiveTexture(GL_TEXTURE0);
            glUniform1i(find_uniform(step.data.uniformtex.shader, step.data.uniformtex.name), step.data.uniformtex.id);
            delete[] step.data.uniformtex.name;
            if (sh != step.data.uniformtex.shader) glUseProgram(find_shader(sh));
            return;
        }
        case STEP_SET_LIGHT: {
            lightx = step.data.set_light.x;
            lighty = step.data.set_light.y;
            lightz = step.data.set_light.z;
            if (mode3d) glUniform3f(find_uniform("light"), lightx, lighty, lightz);
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

extern "C" void LIBDRAW_SYMBOL(opacity)(Opacity o) {
    Step step;
    step.type = STEP_OPACITY;
    step.data.opacity = { o };
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
    char* dup = new char[strlen(str) + 1];
    memcpy(dup, str, strlen(str) + 1);
    step.data.text = { x, y, dup };
    enqueue(step);
}

extern "C" void LIBDRAW_SYMBOL(wraptext)(float x, float y, const char* str, int width) {
    Step step;
    step.type = STEP_WRAPPED_TEXT;
    char* dup = new char[strlen(str) + 1];
    memcpy(dup, str, strlen(str) + 1);
    step.data.wraptext = { x, y, dup, float(width) };
    enqueue(step);
}

// 3D Drawing

extern "C" Texture LIBDRAW_SYMBOL(actex)(Image img) {
    return Texture{ LIBDRAW_CONST(AUTO_CUBE), img, img, img };
}

extern "C" Texture LIBDRAW_SYMBOL(sctex)(Image img) {
    return Texture{ LIBDRAW_CONST(STRETCH_CUBE), img, img, img };
}

extern "C" Texture LIBDRAW_SYMBOL(aptex)(Image sides, Image ends) {
    return Texture{ LIBDRAW_CONST(AUTO_PILLAR), sides, ends, ends };
}

extern "C" Texture LIBDRAW_SYMBOL(sptex)(Image sides, Image ends) {
    return Texture{ LIBDRAW_CONST(STRETCH_PILLAR), sides, ends, ends };
}

extern "C" Texture LIBDRAW_SYMBOL(astex)(Image sides, Image top, Image bottom) {
    return Texture{ LIBDRAW_CONST(AUTO_SIDED), sides, top, bottom };
}

extern "C" Texture LIBDRAW_SYMBOL(sstex)(Image sides, Image top, Image bottom) {
    return Texture{ LIBDRAW_CONST(STRETCH_SIDED), sides, top, bottom };
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

// Multipass

extern "C" void LIBDRAW_SYMBOL(uniformi)(Shader shader, const char* name, int i) {
    Step step;
    step.type = STEP_UNIFORMI;
    char* dup = new char[strlen(name) + 1];
    memcpy(dup, name, strlen(name) + 1);
    step.data.uniformi = { shader, dup, i };
    enqueue(step);
}

extern "C" void LIBDRAW_SYMBOL(uniformf)(Shader shader, const char* name, float f) {
    Step step;
    step.type = STEP_UNIFORMF;
    char* dup = new char[strlen(name) + 1];
    memcpy(dup, name, strlen(name) + 1);
    step.data.uniformf = { shader, dup, f };
    enqueue(step);
}

extern "C" void LIBDRAW_SYMBOL(uniformv2)(Shader shader, const char* name, float x, float y) {
    Step step;
    step.type = STEP_UNIFORMV2;
    char* dup = new char[strlen(name) + 1];
    memcpy(dup, name, strlen(name) + 1);
    step.data.uniformv2 = { shader, dup, x, y };
    enqueue(step);
}

extern "C" void LIBDRAW_SYMBOL(uniformv3)(Shader shader, const char* name, float x, float y, float z) {
    Step step;
    step.type = STEP_UNIFORMV3;
    char* dup = new char[strlen(name) + 1];
    memcpy(dup, name, strlen(name) + 1);
    step.data.uniformv3 = { shader, dup, x, y, z };
    enqueue(step);
}

extern "C" void LIBDRAW_SYMBOL(uniformv4)(Shader shader, const char* name, float x, float y, float z, float w) {
    Step step;
    step.type = STEP_UNIFORMV4;
    char* dup = new char[strlen(name) + 1];
    memcpy(dup, name, strlen(name) + 1);
    step.data.uniformv4 = { shader, dup, x, y, z, w };
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

extern "C" float LIBDRAW_SYMBOL(lightdirx)() {
    return lightx;
}

extern "C" float LIBDRAW_SYMBOL(lightdiry)() {
    return lighty;
}

extern "C" float LIBDRAW_SYMBOL(lightdirz)() {
    return lightz;
}

extern "C" void LIBDRAW_SYMBOL(setlightdir)(float x, float y, float z) {
    Step step;
    step.type = STEP_SET_LIGHT;
    step.data.set_light = { x, y, z };
    enqueue(step);
}

void apply_default_uniforms() {
    glUniform1i(find_uniform("width"), width(currentfbo()));
    glUniform1i(find_uniform("height"), height(currentfbo()));
    glUniform1f(find_uniform("near"), near);
    glUniform1f(find_uniform("far"), far);
    mode3d ? glUniform3f(find_uniform("light"), lightx, lighty, lightz) : glUniform3f(find_uniform("light"), 0, 0, 1);
    glUniformMatrix4fv(find_uniform("projection"), 1, GL_FALSE, (const GLfloat*)projection);
    glUniformMatrix4fv(find_uniform("view"), 1, GL_FALSE, (const GLfloat*)view);
    glUniformMatrix4fv(find_uniform("model"), 1, GL_FALSE, (const GLfloat*)transform);
    glUniform1i(find_uniform("tex"), 0);
}