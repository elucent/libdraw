#include "draw.h"
#include "math.h"
#include "stdlib.h"

static float pi = 3.14159265358979323f;

const char* DEPTH_FSH = R"(
    #version 330
    in vec4 v_col;
    in vec2 v_uv;
    in vec4 v_spr;
    uniform int width, height;
    uniform float near, far;
    uniform sampler2D tex;
    out vec4 color;

    vec2 dims;

    void main() {
        float depth = gl_FragCoord.z;
        depth -= 0.5;
        depth *= 10; 
        depth += 0.5;
        color = vec4(fract(depth * 256), depth - fract(depth * 256), 0, 1);
    }
)";

int main(int argc, char** argv) {
    srand(0);
    window(480, 320, "My Window");
    float yaw = 0, pitch = 0;
    float x = 0, y = 8, z = 0;
    float pmx = 0, pmy = 0;

    Image block = image("asset/block.png");
    origin(FRONT_TOP_LEFT);
    cube(-160, -16, -160, 320, 16, 320, actex(block));

    origin(CENTER);
    Model world = sketch();

    Image fontimg = image("asset/font.png");
    Image bg = image("asset/sunset.png");
    Image depthbuf = newimage(960, 640);
    Shader depth = shader(DEFAULT_VSH, DEPTH_FSH);
    font(fontimg);
    while (running()) {
        origin(CENTER);

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
        ortho(width(SCREEN), height(SCREEN));
        lookat(0, 0, 0, lightdirx(), lightdiry(), lightdirz());
        pan(width(SCREEN) / 2, height(SCREEN) / 2, 0);

        // draw scene
        render(world, block);

        color(RED);
        cube(0, 0, 0, 16, 4, 4, sctex(BLANK));
        cube(8, 0, 0, 4, 6, 6, sctex(BLANK));
        color(BLUE);
        cube(0, 0, 0, 4, 4, 16, sctex(BLANK));
        cube(0, 0, 8, 6, 6, 4, sctex(BLANK));
        color(WHITE);

        shade(depthbuf, depth);

        ortho(width(SCREEN), height(SCREEN));
        snap(0, 0, 0);
        // color(WHITE);
        rect(0, 0, width(SCREEN), height(SCREEN));
        sprite(width(SCREEN) / 2, height(SCREEN) / 2, depthbuf);
    }
    return 0;
}