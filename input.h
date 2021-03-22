#ifndef _LIBDRAW_INPUT_H
#define _LIBDRAW_INPUT_H

#include "draw.h"
#include "lib/GLFW/glfw3.h"

void init_input();
void update_input(GLFWwindow* window, int x, int y, int scale);

#endif