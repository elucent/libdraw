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

    void main() {
        vec2 dims = vec2(width, height);
        if (texture2D(tex, gl_FragCoord.xy / dims).a < 0.01) {
            vec4 right = texture2D(tex, (gl_FragCoord.xy + vec2(1, 0)) / dims),    
                down = texture2D(tex, (gl_FragCoord.xy + vec2(0, 1)) / dims),
                left = texture2D(tex, (gl_FragCoord.xy + vec2(-1, 0)) / dims),
                up = texture2D(tex, (gl_FragCoord.xy + vec2(0, -1)) / dims);
            if (right.a + down.a + left.a + up.a > 1.99) color = vec4(0, 0, 0, 1);
        }
        else color = texture2D(tex, gl_FragCoord.xy / dims);
    }
)";

int main(int argc, char** argv) {
    srand(0);
    window(240, 160, "My Window");
    Shader outline = shader(DEFAULT_VSH, OUTLINE_FSH);
    Image sheet = image("asset/grass.png"), block = subimage(sheet, 0, 32, 64, 48), slab = subimage(sheet, 0, 0, 48, 32), 
        log = subimage(sheet, 64, 0, 48, 48), smlog = subimage(sheet, 64, 48, 24, 32), bark = subimage(sheet, 64, 16, 48, 16);
    Image bg = image("asset/sunset.png"), imp = image("asset/imp.png"),
        temp = newimage(width(SCREEN), height(SCREEN)), temp2 = newimage(width(SCREEN), height(SCREEN));
    Image fontimg = image("asset/font.png");
    float yaw = 0, pitch = 0;
    float x = 0, y = 8, z = 0;
    float pmx = 0, pmy = 0;
    origin(FRONT_TOP_LEFT);
    for (int i = -10; i < 10; i ++) for (int j = -10; j < 10; j ++)
        if ((i - j) % 2 == 0) cube(i * 16, -16, j * 16, 16, 16, 16, block);
        else cube(i * 16, -16, j * 16, 16, 8, 16, slab);
    color(RED);
    cube(-64, 8, -32, 16, 16, 16, block);
    color(GREEN);
    cube(64, 8, -32, 16, 16, 16, block);
    origin(CENTER);
    font(fontimg);
    Model world = sketch();
    while (running()) {
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

        beginstate();
            rotate(45, X_AXIS);
            translate(64, 16, -64);
            origin(CENTER);
            prism(0, 0, 0, 16, 16, 16, 32, X_AXIS, log);
            pyramid(0, 12, 0, 8, 16, 8, 32, DIR_UP, smlog);
            pyramid(0, -12, 0, 8, 16, 8, 32, DIR_DOWN, smlog);
            pyramid(0, 0, 12, 8, 8, 16, 32, DIR_BACK, smlog);
            pyramid(0, 0, -12, 8, 8, 16, 32, DIR_FRONT, smlog);
        endstate();

        ortho(width(SCREEN), height(SCREEN));
        snap(0, 0, 0);
        color(BLACK);
        wraptext(2, 3, R"(ABC DEF GHI JKL MNO PQR STU VWX YZ  abc def ghi jkl mno pqr stu vwx yz  .!? :;, The Quick Brown Fox Jumped Over The Lazy Dog.
)", 128);
        color(WHITE);
        wraptext(2, 2, R"(ABC DEF GHI JKL MNO PQR STU VWX YZ  abc def ghi jkl mno pqr stu vwx yz  .!? :;, The Quick Brown Fox Jumped Over The Lazy Dog.
)", 128);
    }
    return 0;
}