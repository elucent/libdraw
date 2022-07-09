#include "draw.h"
#include "math.h"
#include "stdlib.h"

static float pi = 3.14159265358979323f;

int main(int argc, char** argv) {
    srand(0);
    window(240, 160, "My Window");
    float yaw = 0, pitch = 0;
    float x = 0, y = 8, z = 0;
    float pmx = 0, pmy = 0;

    Image block = image("asset/block.png");
    origin(FRONT_TOP_LEFT);
    cube(-64, -16, -64, 128, 16, 128, actex(block));

    origin(CENTER);
    Model world = sketch();

    Image fontimg = image("asset/font.png");
    Image bg = image("asset/sunset.png");
    font(fontimg);
    while (running()) {
        origin(CENTER);
        sprite(width(SCREEN) / 2, height(SCREEN) / 2, bg);

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
        render(world, block);

        color(RED);
        cube(0, 0, 0, 16, 4, 4, sctex(BLANK));
        cube(8, 0, 0, 4, 6, 6, sctex(BLANK));
        color(BLUE);
        cube(0, 0, 0, 4, 4, 16, sctex(BLANK));
        cube(0, 0, 8, 6, 6, 4, sctex(BLANK));
        color(WHITE);

        ortho(width(SCREEN), height(SCREEN));
        snap(0, 0, 0);
    }
    return 0;
}