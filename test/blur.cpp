#include "draw.h"
#include "math.h"
#include "stdlib.h"

static float pi = 3.14159265358979323f;

const char* BLUR_FSH = R"(
    #version 330
    in vec4 v_col;
    in vec2 v_uv;
    in vec4 v_spr;
    uniform int width, height;
    uniform sampler2D tex;
    out vec4 color;

    vec2 dims;

    const int kernel[25] = int[25](
        1, 4, 7, 4, 1,
        4, 16, 26, 16, 4,
        7, 26, 41, 26, 7,
        4, 16, 26, 16, 4,
        1, 4, 7, 4, 1
    );

    vec2 neighbor(int dx, int dy) {
        vec2 diff = vec2(dx, dy);
        vec2 result = gl_FragCoord.xy + diff;
        result.x = clamp(result.x, 0.5, dims.x - 0.5);
        result.y = clamp(result.y, 0.5, dims.y - 0.5);
        return result / dims;
    }

    void main() {
        dims = vec2(width, height);
        color = vec4(0);
        float rgbweight = 0, aweight = 0;
        for (int i = -2; i < 3; i ++) for (int j = -2; j < 3; j ++) {
            vec4 pix = texture2D(tex, neighbor(i, j));
            int weight = kernel[(i + 2) * 5 + j + 2];
            aweight += weight;
            color.a += pix.a * weight;
            if (pix.a > 0.01) {
                rgbweight += weight;
                color.rgb += pix.rgb * pix.a * weight;
            }
        }
        color.a /= aweight;
        color.rgb /= rgbweight;
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
    cube(-64, -16, -64, 128, 16, 128, actex(block));

    origin(CENTER);
    Model world = sketch();

    Image fontimg = image("asset/font.png");
    Image bg = image("asset/sunset.png");
    Image rendered = newimage(480, 320), blurred1 = newimage(480, 320), blurred2 = newimage(480, 320);
    Shader blur = shader(DEFAULT_VSH, BLUR_FSH);
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

        paint(rendered);

        ortho(width(SCREEN), height(SCREEN));
        snap(0, 0, 0);
        sprite(width(SCREEN) / 2, height(SCREEN) / 2, rendered);

        shade(blurred1, blur);
        sprite(width(SCREEN) / 2, height(SCREEN) / 2, bg);
        sprite(width(SCREEN) / 2, height(SCREEN) / 2, blurred1);
    }
    return 0;
}