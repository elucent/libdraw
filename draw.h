// Copyright (c) 2021 elucent

// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.

// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:

// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.

#ifndef _LIB_DRAW_H
#define _LIB_DRAW_H

#ifdef LIBDRAW_NAMESPACE
    #define LIBDRAW_SYMBOL(sym) libdraw_##sym
    #define LIBDRAW_CONST(sym) LIBDRAW_##sym
#else
    #define LIBDRAW_SYMBOL(sym) sym
    #define LIBDRAW_CONST(sym) sym
#endif

// Windows

extern "C" void LIBDRAW_SYMBOL(window)(int width, int height, const char* title);
extern "C" bool LIBDRAW_SYMBOL(running)();
extern "C" int LIBDRAW_SYMBOL(frames)();
extern "C" double LIBDRAW_SYMBOL(seconds)();

// Images

using Image = int;
extern "C" Image LIBDRAW_SYMBOL(image)(const char* path);
extern "C" Image LIBDRAW_SYMBOL(newimage)(int width, int height);
extern "C" Image LIBDRAW_SYMBOL(subimage)(Image i, int x, int y, int width, int height);
extern "C" int LIBDRAW_SYMBOL(width)(Image i);
extern "C" int LIBDRAW_SYMBOL(height)(Image i);
extern "C" Image LIBDRAW_CONST(SCREEN);
extern "C" Image LIBDRAW_CONST(BLANK);

enum TextureType {
    LIBDRAW_CONST(TEX_NET),
    LIBDRAW_CONST(TEX_AUTO),
    LIBDRAW_CONST(TEX_STRETCH)
};

struct Texture {
    Image img : 28;
    Image img2 : 28;
    TextureType type : 8;
};

extern "C" Texture LIBDRAW_SYMBOL(nettex)(Image img);
extern "C" Texture LIBDRAW_SYMBOL(autotex)(Image img);
extern "C" Texture LIBDRAW_SYMBOL(stretchtex)(Image img);

// Colors

using Color = int;
extern "C" const Color
    LIBDRAW_CONST(RED), LIBDRAW_CONST(ORANGE), LIBDRAW_CONST(YELLOW), LIBDRAW_CONST(LIME), 
    LIBDRAW_CONST(GREEN), LIBDRAW_CONST(CYAN), LIBDRAW_CONST(BLUE), LIBDRAW_CONST(INDIGO), 
    LIBDRAW_CONST(PURPLE), LIBDRAW_CONST(MAGENTA), LIBDRAW_CONST(PINK), LIBDRAW_CONST(BROWN), 
    LIBDRAW_CONST(WHITE), LIBDRAW_CONST(GRAY), LIBDRAW_CONST(BLACK), LIBDRAW_CONST(CLEAR);
extern "C" Color LIBDRAW_SYMBOL(rgba)(int red, int green, int blue, int alpha);
extern "C" Color LIBDRAW_SYMBOL(rgb)(int red, int green, int blue);
extern "C" void LIBDRAW_SYMBOL(color)(Color c);

// 2D Drawing

extern "C" void LIBDRAW_SYMBOL(rect)(float x, float y, float width, float height);
extern "C" void LIBDRAW_SYMBOL(polygon)(float x, float y, float radius, int sides);
extern "C" void LIBDRAW_SYMBOL(circle)(float x, float y, float radius);
extern "C" void LIBDRAW_SYMBOL(sprite)(float x, float y, Image i);
extern "C" void LIBDRAW_SYMBOL(font)(Image i);
extern "C" void LIBDRAW_SYMBOL(text)(float x, float y, const char* str);
extern "C" void LIBDRAW_SYMBOL(wraptext)(float x, float y, const char* str, int width);

// Transformation

enum Origin {
    LIBDRAW_CONST(FRONT_TOP_LEFT) = 0,
    LIBDRAW_CONST(FRONT_TOP_CENTER) = 1,
    LIBDRAW_CONST(FRONT_TOP_RIGHT) = 2,
    LIBDRAW_CONST(FRONT_CENTER_LEFT) = 3,
    LIBDRAW_CONST(FRONT_CENTER) = 4,
    LIBDRAW_CONST(FRONT_CENTER_RIGHT) = 5,
    LIBDRAW_CONST(FRONT_BOTTOM_LEFT) = 6,
    LIBDRAW_CONST(FRONT_BOTTOM_CENTER) = 7,
    LIBDRAW_CONST(FRONT_BOTTOM_RIGHT) = 8,
    LIBDRAW_CONST(TOP_LEFT) = 9,
    LIBDRAW_CONST(TOP_CENTER) = 10,
    LIBDRAW_CONST(TOP_RIGHT) = 11,
    LIBDRAW_CONST(CENTER_LEFT) = 12,
    LIBDRAW_CONST(CENTER) = 13,
    LIBDRAW_CONST(CENTER_RIGHT) = 14,
    LIBDRAW_CONST(BOTTOM_LEFT) = 15,
    LIBDRAW_CONST(BOTTOM_CENTER) = 16,
    LIBDRAW_CONST(BOTTOM_RIGHT) = 17,
    LIBDRAW_CONST(BACK_TOP_LEFT) = 18,
    LIBDRAW_CONST(BACK_TOP_CENTER) = 19,
    LIBDRAW_CONST(BACK_TOP_RIGHT) = 20,
    LIBDRAW_CONST(BACK_CENTER_LEFT) = 21,
    LIBDRAW_CONST(BACK_CENTER) = 22,
    LIBDRAW_CONST(BACK_CENTER_RIGHT) = 23,
    LIBDRAW_CONST(BACK_BOTTOM_LEFT) = 24,
    LIBDRAW_CONST(BACK_BOTTOM_CENTER) = 25,
    LIBDRAW_CONST(BACK_BOTTOM_RIGHT) = 26
};

enum Axis {
    LIBDRAW_CONST(X_AXIS) = 0,
    LIBDRAW_CONST(Y_AXIS) = 1,
    LIBDRAW_CONST(Z_AXIS) = 2
};

enum Direction {
    LIBDRAW_CONST(DIR_LEFT) = 0,
    LIBDRAW_CONST(DIR_RIGHT) = 1,
    LIBDRAW_CONST(DIR_DOWN) = 2,
    LIBDRAW_CONST(DIR_UP) = 3,
    LIBDRAW_CONST(DIR_FRONT) = 4,
    LIBDRAW_CONST(DIR_BACK) = 5
};

enum Edge {
    LIBDRAW_CONST(TOP_LEFT_EDGE) = 0,
    LIBDRAW_CONST(TOP_BACK_EDGE) = 1,
    LIBDRAW_CONST(TOP_RIGHT_EDGE) = 2,
    LIBDRAW_CONST(TOP_FRONT_EDGE) = 3,
    LIBDRAW_CONST(BACK_LEFT_EDGE) = 4,
    LIBDRAW_CONST(BACK_RIGHT_EDGE) = 5,
    LIBDRAW_CONST(FRONT_RIGHT_EDGE) = 6,
    LIBDRAW_CONST(FRONT_LEFT_EDGE) = 7,
    LIBDRAW_CONST(BOTTOM_LEFT_EDGE) = 8,
    LIBDRAW_CONST(BOTTOM_BACK_EDGE) = 9,
    LIBDRAW_CONST(BOTTOM_RIGHT_EDGE) = 10,
    LIBDRAW_CONST(BOTTOM_FRONT_EDGE) = 11
};

extern "C" void LIBDRAW_SYMBOL(origin)(Origin origin);
extern "C" void LIBDRAW_SYMBOL(rotate)(float degrees, Axis axis);
extern "C" void LIBDRAW_SYMBOL(translate)(float dx, float dy, float dz);
extern "C" void LIBDRAW_SYMBOL(scale)(float factor);
extern "C" void LIBDRAW_SYMBOL(scale2d)(float x, float y);
extern "C" void LIBDRAW_SYMBOL(scale3d)(float x, float y, float z);
extern "C" void LIBDRAW_SYMBOL(beginstate)();
extern "C" void LIBDRAW_SYMBOL(endstate)();

// 3D Drawing

extern "C" void LIBDRAW_SYMBOL(cube)(float x, float y, float z, float width, float height, float length, Texture img);
extern "C" void LIBDRAW_SYMBOL(board)(float x, float y, float z, Image i);
extern "C" void LIBDRAW_SYMBOL(slant)(float x, float y, float z, float width, float height, float length, Edge edge, Texture img);
extern "C" void LIBDRAW_SYMBOL(prism)(float x, float y, float z, float width, float height, float length, int sides, Axis axis, Texture img);
extern "C" void LIBDRAW_SYMBOL(cylinder)(float x, float y, float z, float width, float height, float length, Axis axis, Texture img);
extern "C" void LIBDRAW_SYMBOL(pyramid)(float x, float y, float z, float width, float height, float length, int sides, Direction dir, Texture img);
extern "C" void LIBDRAW_SYMBOL(cone)(float x, float y, float z, float width, float height, float length, Direction dir, Texture img);
extern "C" void LIBDRAW_SYMBOL(hedron)(float x, float y, float z, float width, float height, float length, int hsides, int vsides, Texture img);
extern "C" void LIBDRAW_SYMBOL(sphere)(float x, float y, float z, float width, float height, float length, Texture img);

// Camera

extern "C" void LIBDRAW_SYMBOL(ortho)(float width, float height);
extern "C" void LIBDRAW_SYMBOL(frustum)(float width, float height, float fov);
extern "C" void LIBDRAW_SYMBOL(pan)(float x, float y, float z);
extern "C" void LIBDRAW_SYMBOL(tilt)(float degrees, Axis axis);
extern "C" void LIBDRAW_SYMBOL(snap)(float x, float y, float z);
extern "C" void LIBDRAW_SYMBOL(look)(float x, float y, float z, float yaw, float pitch);
extern "C" void LIBDRAW_SYMBOL(lookat)(float x1, float y1, float z1, float x2, float y2, float z2);

// Models

using Model = int;

extern "C" Model LIBDRAW_SYMBOL(createmodel)();
extern "C" Model LIBDRAW_SYMBOL(sketch)();
extern "C" void LIBDRAW_SYMBOL(sketchto)(Model model);
extern "C" void LIBDRAW_SYMBOL(flush)();
// TODO : extern "C" Model LIBDRAW_SYMBOL(loadobj)(const char* path);
extern "C" void LIBDRAW_SYMBOL(render)(Model model, Image img);

// Effects

using Shader = int;

extern "C" const char* LIBDRAW_CONST(DEFAULT_VSH);
extern "C" const char* LIBDRAW_CONST(DEFAULT_FSH);
extern "C" Shader LIBDRAW_CONST(DEFAULT_SHADER);

extern "C" Shader LIBDRAW_SYMBOL(shader)(const char* vsh, const char* fsh);
extern "C" void LIBDRAW_SYMBOL(paint)(Image img);
extern "C" void LIBDRAW_SYMBOL(shade)(Image img, Shader shader);
extern "C" void LIBDRAW_SYMBOL(fog)(Color color, float range);
extern "C" void LIBDRAW_SYMBOL(nofog)();

// Input

enum MouseButton {
    LIBDRAW_CONST(LEFT_CLICK) = 0,
    LIBDRAW_CONST(RIGHT_CLICK) = 1
};

extern "C" bool LIBDRAW_SYMBOL(keytap)(const char* key);
extern "C" bool LIBDRAW_SYMBOL(keydown)(const char* key);
extern "C" bool LIBDRAW_SYMBOL(mousetap)(MouseButton button);
extern "C" bool LIBDRAW_SYMBOL(mousedown)(MouseButton button);
extern "C" float LIBDRAW_SYMBOL(mousex)();
extern "C" float LIBDRAW_SYMBOL(mousey)();
extern "C" float LIBDRAW_SYMBOL(scroll)();
extern "C" void LIBDRAW_SYMBOL(setmouse)(float x, float y);
extern "C" void LIBDRAW_SYMBOL(hidemouse)();
extern "C" void LIBDRAW_SYMBOL(showmouse)();

#endif