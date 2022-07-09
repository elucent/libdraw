#include "draw.h"
#include "math.h"
#include "stdlib.h"

static float pi = 3.14159265358979323f;

const char* OUTLINE_FSH = R"(
    #version 330
    in vec4 v_col;
    in vec2 v_uv;
    in vec4 v_spr;
    uniform int width, height;
    uniform sampler2D tex;
    out vec4 color;

    vec2 dims;

    vec2 neighbor(int dx, int dy) {
        vec2 diff = vec2(dx, dy);
        vec2 result = gl_FragCoord.xy + diff;
        result.x = clamp(result.x, 0.5, dims.x - 0.5);
        result.y = clamp(result.y, 0.5, dims.y - 0.5);
        return result / dims;
    }

    void main() {
        dims = vec2(width, height);
        vec4 col = texture2D(tex, gl_FragCoord.xy / dims);
        if (col.a < 0.01) {
            float right = texture2D(tex, neighbor(1, 0)).a,
                  left = texture2D(tex, neighbor(-1, 0)).a,
                  up = texture2D(tex, neighbor(0, -1)).a,
                  down = texture2D(tex, neighbor(0, 1)).a;
            if (right > 0.01 && right <= 1 || left > 0.01 && left <= 1
                || up > 0.01 && up <= 1 || down > 0.01 && down <= 1)
                color = vec4(0, 0, 0, 1);
            else discard;
        }
        else color = texture2D(tex, gl_FragCoord.xy / dims);
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
    Image rendered = newimage(480, 320), outlined = newimage(480, 320);
    Shader outline = shader(DEFAULT_VSH, OUTLINE_FSH);
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

        shade(outlined, outline);
        
        sprite(width(SCREEN) / 2, height(SCREEN) / 2, bg);
        sprite(width(SCREEN) / 2, height(SCREEN) / 2, outlined);
    }
    return 0;
}