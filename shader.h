#ifndef _LIBDRAW_SHADER_H
#define _LIBDRAW_SHADER_H

#include "draw.h"
#include "lib/GLAD/glad.h"
#include "lib/util/str.h"

GLuint find_shader(Shader shader);
void init_shaders();
GLint find_uniform(const string& name);
Shader active_shader();
void bind(Shader shader);

#endif