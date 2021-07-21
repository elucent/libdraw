#include "input.h"
#include "lib/GLFW/glfw3.h"
#include "lib/util/hash.h"
#include "lib/util/io.h"

static bool pressed[512];
static bool tapped[512];
static int codes[128];
static map<string, int> longcodes;
static float mx, my;
static float mouse_scroll = 0;
static int winx, winy, winscale;

static bool leftmousetapped = false,
        rightmousetapped = false,
        leftmousepressed = false,
        rightmousepressed = false;

static GLFWwindow* the_window;

static void initcodes() {
    codes['a'] = codes['A'] = GLFW_KEY_A;
    codes['b'] = codes['B'] = GLFW_KEY_B;
    codes['c'] = codes['C'] = GLFW_KEY_C;
    codes['d'] = codes['D'] = GLFW_KEY_D;
    codes['e'] = codes['E'] = GLFW_KEY_E;
    codes['f'] = codes['F'] = GLFW_KEY_F;
    codes['g'] = codes['G'] = GLFW_KEY_G;
    codes['h'] = codes['H'] = GLFW_KEY_H;
    codes['i'] = codes['I'] = GLFW_KEY_I;
    codes['j'] = codes['J'] = GLFW_KEY_J;
    codes['k'] = codes['K'] = GLFW_KEY_K;
    codes['l'] = codes['L'] = GLFW_KEY_L;
    codes['m'] = codes['M'] = GLFW_KEY_M;
    codes['n'] = codes['N'] = GLFW_KEY_N;
    codes['o'] = codes['O'] = GLFW_KEY_O;
    codes['p'] = codes['P'] = GLFW_KEY_P;
    codes['q'] = codes['Q'] = GLFW_KEY_Q;
    codes['r'] = codes['R'] = GLFW_KEY_R;
    codes['s'] = codes['S'] = GLFW_KEY_S;
    codes['t'] = codes['T'] = GLFW_KEY_T;
    codes['u'] = codes['U'] = GLFW_KEY_U;
    codes['v'] = codes['V'] = GLFW_KEY_V;
    codes['w'] = codes['W'] = GLFW_KEY_W;
    codes['x'] = codes['X'] = GLFW_KEY_X;
    codes['y'] = codes['Y'] = GLFW_KEY_Y;
    codes['z'] = codes['Z'] = GLFW_KEY_Z;
    codes['-'] = GLFW_KEY_MINUS;
    codes['='] = GLFW_KEY_EQUAL;
    codes['1'] = GLFW_KEY_1;
    codes['2'] = GLFW_KEY_2;
    codes['3'] = GLFW_KEY_3;
    codes['4'] = GLFW_KEY_4;
    codes['5'] = GLFW_KEY_5;
    codes['6'] = GLFW_KEY_6;
    codes['7'] = GLFW_KEY_7;
    codes['8'] = GLFW_KEY_8;
    codes['9'] = GLFW_KEY_9;
    codes['0'] = GLFW_KEY_0;
    codes['`'] = GLFW_KEY_GRAVE_ACCENT;
    codes['['] = GLFW_KEY_LEFT_BRACKET;
    codes[']'] = GLFW_KEY_RIGHT_BRACKET;
    codes['\\'] = GLFW_KEY_BACKSLASH;
    codes[';'] = GLFW_KEY_SEMICOLON;
    codes['\''] = GLFW_KEY_APOSTROPHE;
    codes[','] = GLFW_KEY_COMMA;
    codes['.'] = GLFW_KEY_PERIOD;
    codes['/'] = GLFW_KEY_SLASH;
    codes[' '] = GLFW_KEY_SPACE;
    codes['\t'] = GLFW_KEY_TAB;
    codes['\n'] = GLFW_KEY_ENTER;
    codes['\b'] = GLFW_KEY_BACKSPACE;

    longcodes["up"] = GLFW_KEY_UP;
    longcodes["down"] = GLFW_KEY_DOWN;
    longcodes["left"] = GLFW_KEY_LEFT;
    longcodes["right"] = GLFW_KEY_RIGHT;
    longcodes["space"] = GLFW_KEY_SPACE;
    longcodes["enter"] = GLFW_KEY_ENTER;
    longcodes["shift"] = longcodes["left shift"] = GLFW_KEY_LEFT_SHIFT;
    longcodes["right shift"] = GLFW_KEY_RIGHT_SHIFT;
    longcodes["control"] = longcodes["left control"] = GLFW_KEY_LEFT_CONTROL;
    longcodes["right control"] = GLFW_KEY_RIGHT_CONTROL;
    longcodes["escape"] = GLFW_KEY_ESCAPE;
    longcodes["backspace"] = GLFW_KEY_BACKSPACE;
    longcodes["delete"] = GLFW_KEY_DELETE;
    longcodes["tab"] = GLFW_KEY_TAB;
    longcodes["caps lock"] = GLFW_KEY_CAPS_LOCK;
    longcodes["alt"] = longcodes["left alt"] = GLFW_KEY_LEFT_ALT;
    longcodes["right alt"] = GLFW_KEY_RIGHT_ALT;
    longcodes["f1"] = GLFW_KEY_F1;
    longcodes["f2"] = GLFW_KEY_F2;
    longcodes["f3"] = GLFW_KEY_F3;
    longcodes["f4"] = GLFW_KEY_F4;
    longcodes["f5"] = GLFW_KEY_F5;
    longcodes["f6"] = GLFW_KEY_F6;
    longcodes["f7"] = GLFW_KEY_F7;
    longcodes["f8"] = GLFW_KEY_F8;
    longcodes["f9"] = GLFW_KEY_F9;
    longcodes["f10"] = GLFW_KEY_F10;
    longcodes["f11"] = GLFW_KEY_F11;
    longcodes["f12"] = GLFW_KEY_F12;
    longcodes["print screen"] = GLFW_KEY_PRINT_SCREEN;
    longcodes["home"] = GLFW_KEY_HOME;
    longcodes["end"] = GLFW_KEY_END;
    longcodes["page up"] = GLFW_KEY_PAGE_UP;
    longcodes["page down"] = GLFW_KEY_PAGE_DOWN;
}

static void updatekey(GLFWwindow* window, int code) {
    the_window = window;
    int status = glfwGetKey(window, code);
    if (status == GLFW_PRESS) {
        if (!pressed[code]) tapped[code] = true;
        else tapped[code] = false;
        pressed[code] = true;
    }
    else if (status == GLFW_RELEASE) {
        pressed[code] = tapped[code] = false;
    }
}

static void updatemouse(GLFWwindow* window, int code) {
    the_window = window;
    int status = glfwGetMouseButton(window, code);
    if (status == GLFW_PRESS) {
        if (code == GLFW_MOUSE_BUTTON_LEFT) {
            if (!leftmousepressed) leftmousetapped = true;
            else leftmousetapped = false;
            leftmousepressed = true;
        }
        else if (code == GLFW_MOUSE_BUTTON_RIGHT) {
            if (!rightmousepressed) rightmousetapped = true;
            else rightmousetapped = false;
            rightmousepressed = true;
        }
    }
    else if (status == GLFW_RELEASE) {
        if (code == GLFW_MOUSE_BUTTON_LEFT) {
            leftmousepressed = leftmousetapped = false;
        }
        else if (code == GLFW_MOUSE_BUTTON_RIGHT) {
            rightmousepressed = rightmousetapped = false;
        }
    }
}

void update_input(GLFWwindow* window, int x, int y, int scale) {
    mouse_scroll = 0;
    glfwPollEvents();
    for (int i = 0; i < 512; i ++)
        updatekey(window, i);
    updatemouse(window, GLFW_MOUSE_BUTTON_LEFT); 
    updatemouse(window, GLFW_MOUSE_BUTTON_RIGHT);

    double dmx, dmy;
    glfwGetCursorPos(the_window, &dmx, &dmy);
    mx = ((float)dmx - (float)x) / (float)scale;
    my = ((float)dmy - (float)y) / (float)scale;
    winx = x, winy = y, winscale = scale;
}

void scroll_callback(GLFWwindow* window, double xscroll, double yscroll) {
    mouse_scroll = (float)yscroll;
}

void init_input(GLFWwindow* window) {
    for (int i = 0; i < 512; i ++) 
        pressed[i] = false, tapped[i] = false;
    for (int i = 0; i < 128; i ++) 
        codes[i] = 0;
    initcodes();

    glfwSetScrollCallback(window, scroll_callback);
}

extern "C" bool LIBDRAW_SYMBOL(keytap)(const char* key) {
    if (!*key) return false;
    int code = 0;
    if (!key[1]) code = codes[*key];
    else code = longcodes[key];

    return tapped[code];
}

extern "C" bool LIBDRAW_SYMBOL(keydown)(const char* key) {
    if (!*key) return false;
    int code = 0;
    if (!key[1]) code = codes[*key];
    else code = longcodes[key];

    return pressed[code];
}

extern "C" bool LIBDRAW_SYMBOL(mousetap)(MouseButton button) {
    return button == LIBDRAW_CONST(LEFT_CLICK) ? leftmousetapped : rightmousetapped;
}

extern "C" bool LIBDRAW_SYMBOL(mousedown)(MouseButton button) {
    return button == LIBDRAW_CONST(LEFT_CLICK) ? leftmousepressed : rightmousepressed;
}

extern "C" float LIBDRAW_SYMBOL(mousex)() {
    return mx;
}

extern "C" float LIBDRAW_SYMBOL(mousey)() {
    return my;
}

extern "C" float LIBDRAW_SYMBOL(scroll)() {
    return mouse_scroll;
}

extern "C" void LIBDRAW_SYMBOL(setmouse)(float x, float y) {
    mx = x * winscale + winx, my = y * winscale + winy;
    if (the_window) glfwSetCursorPos(the_window, mx, my);
}

extern "C" void LIBDRAW_SYMBOL(hidemouse)() {
    if (the_window) glfwSetInputMode(the_window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
}

extern "C" void LIBDRAW_SYMBOL(showmouse)() {
    if (the_window) glfwSetInputMode(the_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}