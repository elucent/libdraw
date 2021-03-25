#include "draw.h"
#include "math.h"
#include "stdlib.h"

static float pi = 3.14159265358979323f;

#define BOARD_DIM 256

Image wall, ground, ceiling, pillar;
float x = 0, y = 8, z = 0;

bool at(bool* b, int i, int j) {
    if (i < 0 || j < 0 || i >= BOARD_DIM || j >= BOARD_DIM) return true;
    return b[i * BOARD_DIM + j];
}

int neighbors(bool* b, int i, int j) {
    int sum = 0;
    for (int xx = -1; xx < 2; xx ++) {
        for (int yy = -1; yy < 2; yy ++) {
            if (at(b, i + xx, j + yy)) sum ++;
        }
    }
    return sum;
}

void generate() {
    bool noise[BOARD_DIM * BOARD_DIM], temp[BOARD_DIM * BOARD_DIM];
    bool *src = noise, *dst = temp;

    // generate noise
    for (int i = 0; i < BOARD_DIM; i ++) { 
        for (int j = 0; j < BOARD_DIM; j ++) {
            noise[i * BOARD_DIM + j] = rand() % 2 == 0;
        }
    }

    // clear out caverns
    for (int k = 0; k < 4; k ++) { 
        for (int i = 0; i < BOARD_DIM; i ++) {
            for (int j = 0; j < BOARD_DIM; j ++) {
                int n = neighbors(src, i, j) + rand() % 2;
                if (n < 4) dst[i * BOARD_DIM + j] = false;
                else if (n > 7) dst[i * BOARD_DIM + j] = true;
                else dst[i * BOARD_DIM + j] = rand() % 2;
            }
        }
        bool* temp = src;
        src = dst;
        dst = temp;
    }

    // generate world
    for (int i = 0; i < BOARD_DIM; i ++) {
        for (int j = 0; j < BOARD_DIM; j ++) {
            if (src[i * BOARD_DIM + j]) {
                if (!at(src, i - 1, j) && !at(src, i, j - 1)
                    && !at(src, i + 1, j) && !at(src, i, j + 1)) {
                    cube(i * 16, -16, j * 16, 16, 16, 16, ground);
                    cube(i * 16, 32, j * 16, 16, 16, 16, ceiling);
                    prism(i * 16 + 2, 0, j * 16 + 2, 12, 32, 12, 5, Y_AXIS, pillar);
                }
                else cube(i * 16, 0, j * 16, 16, 32, 16, wall);
            }
            else {
                cube(i * 16, -16, j * 16, 16, 16, 16, ground);
                cube(i * 16, 32, j * 16, 16, 16, 16, ceiling);
                if (at(src, i - 1, j) && at(src, i, j - 1)) slant(i * 16, 0, j * 16, 16, 32, 16, FRONT_LEFT_EDGE, wall);
                if (at(src, i + 1, j) && at(src, i, j - 1)) slant(i * 16, 0, j * 16, 16, 32, 16, FRONT_RIGHT_EDGE, wall);
                if (at(src, i + 1, j) && at(src, i, j + 1)) slant(i * 16, 0, j * 16, 16, 32, 16, BACK_RIGHT_EDGE, wall);
                if (at(src, i - 1, j) && at(src, i, j + 1)) slant(i * 16, 0, j * 16, 16, 32, 16, BACK_LEFT_EDGE, wall);
                x = i * 16 + 8, z = j * 16 + 8;
            }
        }
    }
}

int main(int argc, char** argv) {
    srand(0);
    window(240, 160, "Cave Explorer");
    Image sheet = image("asset/cavern_sheet.png");
    wall = subimage(sheet, 0, 0, 96, 80), ground = subimage(sheet, 144, 0, 64, 48), ceiling = subimage(sheet, 80, 0, 64, 48), pillar = subimage(sheet, 48, 48, 36, 56);
    float yaw = 0, pitch = 0;
    float pmx = 0, pmy = 0;
    origin(FRONT_TOP_LEFT);
    generate();
    Model world = sketch();
    while (running()) {
        // camera controls
        hidemouse();
        pmx = (pmx + (mousex() - width(SCREEN) / 2) * 0.5f) / 5;
        pmy = (pmy + (mousey() - height(SCREEN) / 2) * 0.5f) / 5;
        yaw += pmx, pitch -= pmy;
        setmouse(width(SCREEN) / 2, height(SCREEN) / 2);
        if (pitch < -90) pitch = -90;
        if (pitch > 90) pitch = 90;
        if (keydown("w")) x -= sin(pi * -yaw / 180), z -= cos(pi * -yaw / 180);
        if (keydown("s")) x += sin(pi * -yaw / 180), z += cos(pi * -yaw / 180);
        if (keydown("a")) x -= sin(pi * (90 + yaw) / 180), z += cos(pi * (90 + yaw) / 180);
        if (keydown("d")) x += sin(pi * (90 + yaw) / 180), z -= cos(pi * (90 + yaw) / 180);

        // set camera
        frustum(width(SCREEN), height(SCREEN), 70);
        look(x, y, z, yaw, pitch);

        // draw scene
        render(world, sheet);
    }
    return 0;
}