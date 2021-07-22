#include "draw.h"
#include "lib/GLAD/glad.h"
#include "lib/GLFW/glfw3.h"
#include "lib/SOIL/SOIL.h"
#include "lib/util/vec.h"
#include "lib/util/io.h"
#include "stdlib.h"
#include "image.h"
#include "input.h"
#include "queue.h"
#include "shader.h"
#include "fbo.h"
#include "model.h"

namespace internal {
    static GLFWwindow* window = nullptr;
    static vector<GLuint> textures;
    static GLuint vsh = 0, fsh = 0, shprog = 0, vao = 0;
    static GLuint fbo = 0, rbo = 0, fbtex = 0;
    static int frames = 0;
    static double prev_frame_time = 0;

    static int width = 0, height = 0, screenwidth = 0, screenheight = 0;

    static void resize(GLFWwindow* window, int width, int height) {
        internal::screenwidth = width;
        internal::screenheight = height;
    }

    // #ifdef __unix__
    // #include <unistd.h>
    // #elif defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
    // #include <windows.h>
    // #endif

    // static void sleep(int ms) {
    //     #ifdef __unix__
    //     usleep(ms * 1000);
    //     #elif defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
    //     Sleep(ms);
    //     #endif
    // }

    void init() {
        // setup GLFW for window
        glfwMakeContextCurrent(window);
        glfwSwapInterval(1);
        glfwShowWindow(window);

        // setup OpenGL
        gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
        init_input(window);
        init_images(width, height);
        init_shaders();
        init_default_fbo(width, height);
        init_queue();
        // initshaders();
        // initdefaultfbo();
        glUseProgram(find_shader(LIBDRAW_CONST(DEFAULT_SHADER)));
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glEnable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        glActiveTexture(GL_TEXTURE0);
        for (int i = 0; i < 5; i ++) glEnableVertexAttribArray(i);

        if (GLenum err = glGetError()) println("Failed to initialize OpenGL: ", (int)err), exit(1);
    }
}

// Windows

extern "C" void LIBDRAW_SYMBOL(window)(int width, int height, const char* title) {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);
    glfwWindowHint(GLFW_VISIBLE, GL_TRUE);
    internal::width = width;
    internal::height = height;
    internal::screenwidth = internal::width;
    internal::screenheight = internal::height;
    while (internal::screenwidth < 640 && internal::screenheight < 480)
        internal::screenwidth += internal::width, internal::screenheight += internal::height;
    internal::window = glfwCreateWindow(internal::screenwidth, internal::screenheight, title, nullptr, nullptr);
    const char* bufptr;
    if (glfwGetError(&bufptr)) println(bufptr);
    
    glfwSetWindowSizeCallback(internal::window, internal::resize);
    internal::init();
}

static void prelude() {
    identity(transform);
    nofog();
    glUniformMatrix4fv(find_uniform("model"), 1, GL_FALSE, (const GLfloat*)transform);
    ortho(internal::width, internal::height);
    look(0, 0, 0, 0, 0);
    color(LIBDRAW_CONST(WHITE));
    origin(LIBDRAW_CONST(FRONT_TOP_LEFT));
}

extern "C" bool LIBDRAW_SYMBOL(running)() {
    // ending frame
    bindfbo(LIBDRAW_CONST(SCREEN));
    if (GLenum err = glGetError()) println("Failed to clear window: ", (int)err), exit(1);  

    flush(getrendermodel());
    color(WHITE);

    unbindfbo();  
    if (GLenum err = glGetError()) println("Failed to draw frame: ", (int)err), exit(1);
    
    // draw to window surface
    int w = internal::width, h = internal::height;
    while (w + internal::width <= internal::screenwidth && h + internal::height <= internal::screenheight)
        w += internal::width, h += internal::height;
    look(0, 0, 0, 0, 0);
    glViewport(0, 0, internal::screenwidth, internal::screenheight);
    bind(LIBDRAW_CONST(DEFAULT_SHADER));
    ortho(internal::screenwidth, internal::screenheight);
    origin(LIBDRAW_CONST(CENTER));
    stretched_sprite(internal::screenwidth / 2, internal::screenheight / 2, w, h, LIBDRAW_CONST(SCREEN));
    invert = true;
    prelude();
    flush(getrendermodel());
    invert = false;
    glfwSwapBuffers(internal::window);

    // done ending frame
    update_input(internal::window, internal::screenwidth / 2 - w / 2, internal::screenheight / 2 - h / 2, w / internal::width);
    bool closed = glfwWindowShouldClose(internal::window);
    if (closed) return false;

    double frame_time = glfwGetTime();
    double diff = frame_time - internal::prev_frame_time;
    if (diff < 1.0f / 60.0f) {
        double ms = 1.0f / 60.0f - diff;
        ms *= 1000;
        // internal::sleep((int)ms);
    }
    internal::prev_frame_time = glfwGetTime();

    internal::frames ++;
    bindfbo(LIBDRAW_CONST(SCREEN));
    bind(LIBDRAW_CONST(DEFAULT_SHADER));
    prelude();
    return true;
}

extern "C" int LIBDRAW_SYMBOL(frames)() {
    return internal::frames;
}

extern "C" double LIBDRAW_SYMBOL(seconds)() {
    return glfwGetTime();
}

// Images
extern "C" Image LIBDRAW_CONST(WINDOW);

// Colors

const Color
    LIBDRAW_CONST(RED) = rgb(255, 0, 0), 
    LIBDRAW_CONST(ORANGE) = rgb(255, 128, 0), 
    LIBDRAW_CONST(YELLOW) = rgb(255, 255, 0), 
    LIBDRAW_CONST(LIME) = rgb(128, 255, 0), 
    LIBDRAW_CONST(GREEN) = rgb(0, 255, 0), 
    LIBDRAW_CONST(CYAN) = rgb(0, 255, 255), 
    LIBDRAW_CONST(BLUE) = rgb(0, 128, 255), 
    LIBDRAW_CONST(INDIGO) = rgb(0, 0, 255), 
    LIBDRAW_CONST(PURPLE) = rgb(128, 0, 255), 
    LIBDRAW_CONST(MAGENTA) = rgb(255, 0, 255), 
    LIBDRAW_CONST(PINK) = rgb(255, 128, 192), 
    LIBDRAW_CONST(BROWN) = rgb(128, 64, 0), 
    LIBDRAW_CONST(WHITE) = rgb(255, 255, 255),  
    LIBDRAW_CONST(GRAY) = rgb(128, 128, 128), 
    LIBDRAW_CONST(BLACK) = rgb(0, 0, 0),
    LIBDRAW_CONST(CLEAR) = rgba(0, 0, 0, 0);

extern "C" Color LIBDRAW_SYMBOL(rgb)(int r, int g, int b) {
    return rgba(r, g, b, 255);
}

extern "C" Color LIBDRAW_SYMBOL(rgba)(int r, int g, int b, int a) {
    return (r & 255) << 24 | (g & 255) << 16 | (b & 255) << 8 | a & 255;
}