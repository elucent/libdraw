#include "shader.h"
#include "lib/util/str.h"
#include "lib/util/io.h"
#include "lib/util/vec.h"
#include "lib/util/hash.h"
#include "image.h"
#include "queue.h"

static vector<GLuint> shaders;
static vector<map<string, GLint>> uniforms;

const char* LIBDRAW_CONST(DEFAULT_VSH) = R"(
    #version 330
    layout(location=0) in vec3 pos;
    layout(location=1) in vec4 col;
    layout(location=2) in vec3 norm;
    layout(location=3) in vec2 uv;
    layout(location=4) in vec4 spr;

    uniform int width, height;
    uniform mat4 model, view, projection;
    uniform vec3 light;
    uniform int inverted;

    out vec4 v_col;
    out vec2 v_uv;
    out vec4 v_spr;
    out vec4 v_pos;

    void main() {
        v_pos = model * vec4(pos, 1);
        gl_Position = projection * view * v_pos;
        float bright = (-dot(light, normalize(mat3(model) * norm)) + 2) / 3;
        v_col = vec4(bright * col.rgb, col.a);
        v_uv = uv;
        v_spr = vec4(spr.x, spr.y, spr.z, spr.w);
    }
)";

const char* LIBDRAW_CONST(DEFAULT_FSH) = R"(
    #version 330
    in vec4 v_col;
    in vec2 v_uv;
    in vec4 v_spr;
    in vec4 v_pos;

    uniform sampler2D tex;
    uniform vec4 fog_color;
    uniform float fog_range;

    out vec4 color;

    void main() {
        vec2 fract_uv = fract(v_uv);
        vec2 fixed_uv = vec2(v_spr.x + v_spr.z * fract_uv.x, v_spr.y + v_spr.w * fract_uv.y);
        color = v_col * texture2D(tex, fixed_uv);
        if (color.a <= 0.00390625) discard;

        if (fog_range > 0.01) {
            float v_dist = sqrt(v_pos.x * v_pos.x + v_pos.y * v_pos.y + v_pos.z * v_pos.z);
            float fog_density = clamp((fog_range - v_dist) / fog_range, 0, 1);
            color = vec4(mix(fog_color.rgb, color.rgb, fog_density), color.a);
        }
    }
)";

Shader LIBDRAW_CONST(DEFAULT_SHADER);

extern Shader LIBDRAW_SYMBOL(shader)(const char* vsrc, const char* fsrc) {
    GLuint vsh = glCreateShader(GL_VERTEX_SHADER);
    GLuint fsh = glCreateShader(GL_FRAGMENT_SHADER);

    GLint size = string(vsrc).size();
    glShaderSource(vsh, 1, &vsrc, &size);
    size = string(fsrc).size();
    glShaderSource(fsh, 1, &fsrc, &size);

    char log[1024];
    GLsizei length = 0;
    GLint status = GL_TRUE;
    glCompileShader(vsh);
    if (glGetShaderiv(vsh, GL_COMPILE_STATUS, &status), !status) {
        glGetShaderInfoLog(vsh, 1024, &length, log);
        println("Vertex shader error:\n", (const char*)log);
    }
    glCompileShader(fsh);
    if (glGetShaderiv(fsh, GL_COMPILE_STATUS, &status), !status) {
        glGetShaderInfoLog(fsh, 1024, &length, log);
        println("Fragment shader error:\n", (const char*)log);
    }

    GLuint result = glCreateProgram();
    glAttachShader(result, vsh);
    glAttachShader(result, fsh);
    glLinkProgram(result);
    shaders.push(result);
    uniforms.push({});
    return shaders.size() - 1;
}

GLuint find_shader(Shader shader) {
    return shaders[shader];
}

void init_shaders() {
    LIBDRAW_CONST(DEFAULT_SHADER) = shader(LIBDRAW_CONST(DEFAULT_VSH), LIBDRAW_CONST(DEFAULT_FSH));
}

static Shader active;

Shader active_shader() {
    return active;
}

GLint find_uniform(Shader shader, const string& name) {
    auto it = uniforms[shader].find(name);
    if (it != uniforms[shader].end()) return it->second;
    return uniforms[shader][name] = glGetUniformLocation(shaders[shader], (const GLchar*)name.raw());
}

GLint find_uniform(const string& name) {
    return find_uniform(active, name);
}

void bind(Shader shader) {
    const static GLfloat identity[16] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };
    active = shader;
    GLuint id = find_shader(shader);
    glUseProgram(id);

    // default uniforms
    apply_default_uniforms();
}