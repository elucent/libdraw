#include "draw.h"
#include "math.h"
#include "stdlib.h"

static float pi = 3.14159265358979323f;

const char* LIGHT_FSH = R"(
    #version 330
    in vec4 v_pos;
    in vec4 v_col;
    in vec2 v_uv;
    in vec4 v_spr;
    uniform int width, height;
    uniform sampler2D tex;
    uniform vec4 fog_color;
    uniform float fog_range;
    out vec4 color;

    struct Light {
        vec4 color;
        vec3 pos;
        float radius;
    };
    uniform Light lights[2];
    uniform float ambient;
    const int N_LIGHTS = 2;

    void main() {
        // This bit is straight from DEFAULT_FSH
        if (color.a <= 0.00390625) discard;
        vec2 fract_uv = fract(v_uv);
        vec2 fixed_uv = vec2(v_spr.x + v_spr.z * fract_uv.x, v_spr.y + v_spr.w * fract_uv.y);
        color = v_col * texture2D(tex, fixed_uv);

        // This bit handles the dynamic lighting.
        color.rgb *= ambient; // Ambient base.
        vec4 lit = vec4(0);
        for (int i = 0; i < N_LIGHTS; i ++) {
            float dist = sqrt(dot(v_pos.xyz - lights[i].pos, v_pos.xyz - lights[i].pos));
            float bright = clamp(1.0 - dist / lights[i].radius, 0, 1);
            bright *= bright;
            lit.rgb += lights[i].color.rgb * bright * lights[i].color.a;
        }
        color = clamp(lit + color, 0, 1);

        // Fog handled after.
        if (fog_range > 0.01) {
            float v_dist = sqrt(v_pos.x * v_pos.x + v_pos.y * v_pos.y + v_pos.z * v_pos.z);
            float fog_density = clamp((fog_range - v_dist) / fog_range, 0, 1);
            color = vec4(mix(fog_color.rgb, color.rgb, fog_density), color.a);
        }
    }
)";

int main(int argc, char** argv) {
    srand(32);
    window(480, 320, "My Window");
    float yaw = 0, pitch = 0;
    float x = 0, y = 8, z = 0;
    float pmx = 0, pmy = 0;

    Image brick = image("asset/brick.png");
    origin(FRONT_TOP_LEFT);
    cube(-64, -16, -64, 128, 16, 128, actex(brick));
    for (int i = 0; i < 8; i ++) for (int j = 0; j < 8; j ++) {
        int ht = rand() % 3 + 1;
        cone(i * 16 - 64, 0, j * 16 - 64, 16, ht * 16, 16, DIR_UP, actex(brick));
    }
    Model world = sketch();

    Image fontimg = image("asset/font.png");
    Image lightbuf = newimage(width(SCREEN), height(SCREEN)), renderbuf = newimage(width(SCREEN), height(SCREEN));
    font(fontimg);

    Shader lightshdr = shader(DEFAULT_VSH, LIGHT_FSH);
    uniformv4(lightshdr, "lights[0].color", 1, 0.25f, 0.125f, 1);
    uniformv3(lightshdr, "lights[0].pos", -48, 0, -48);
    uniformf(lightshdr, "lights[0].radius", 48);
    uniformv4(lightshdr, "lights[1].color", 0.25f, 0.5f, 1, 2);
    uniformv3(lightshdr, "lights[1].pos", 0, 0, 32);
    uniformf(lightshdr, "lights[1].radius", 96);
    uniformf(lightshdr, "ambient", 0.75f);

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
        if (keydown("space")) y += 1;
        if (keydown("shift")) y -= 1;

        // set camera
        frustum(width(SCREEN), height(SCREEN), 70);
        look(x, y, z, yaw, pitch);

        // draw scene
        render(world, brick);

        // render to texture through custom shader
        shade(lightbuf, lightshdr);
        
        // draw renderbuffer to screen
        ortho(width(SCREEN), height(SCREEN));
        look(0, 0, 0, 0, 0); // reset camera
        sprite(width(SCREEN) / 2, height(SCREEN) / 2, lightbuf);
    }
    return 0;
}