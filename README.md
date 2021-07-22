# Libdraw

Libdraw is a lightweight graphics library for C++ designed with the intention of making
simple graphical programs simple to write. It is a relatively shallow wrapper around OpenGL,
providing a terse but powerful fixed-function interface. 

## Table of Contents

### 1 - Building Libdraw

### 2 - Documentation

 * #### 2.1 - Window Management
   * `window()`
   * `running()`
   * `seconds()`
   * `frames()`

 * #### 2.2 - Images
   * `Image`
   * `image()`
   * `newimage()`
   * `subimage()`
   * `width()`
   * `height()`
   * `BLANK`
   * `SCREEN`
   * `Texture`
   * `nettex()`
   * `autotex()`
   * `stretchtex()`

 * #### 2.3 - Colors
   * `Color`
   * Color Constants
   * `rgba()` / `rgb()`
   * `color()`

 * #### 2.4 - 2D Drawing
   * `rect()`
   * `polygon()` / `circle()`
   * `sprite()`
   * `font()`
   * `text()` / `wraptext()`

 * #### 2.5 - Transformations
   * `Origin`
   * `Axis`
   * `Direction`
   * `Edge`
   * `origin()`
   * `rotate()`
   * `translate()`
   * `scale()` / `scale2d()` / `scale3d()`
   * `beginstate()` / `endstate()`

 * #### 2.6 - 3D Drawing
   * `cube()`
   * `board()`
   * `slant()`
   * `prism()` / `cylinder()`
   * `pyramid()` / `cone()`
   * `hedron()` / `sphere()`

 * #### 2.7 - Camera Controls
   * `ortho()`
   * `frustum()`
   * `pan()`
   * `tilt()`
   * `snap()`
   * `look()` / `lookat()`

 * #### 2.8 - Models
   * `Model`
   * `createmodel()`
   * `sketch()` / `sketchto()`
   * `flush()`
   * `render()`

 * #### 2.9 - Effects
   * `Shader`
   * `DEFAULT_VSH` / `DEFAULT_FSH` / `DEFAULT_SHADER`
   * `shader()`
   * `paint()` / `shade()`
   * `fog()` / `nofog()`

 * #### 2.10 - Input
   * `keytap()` / `keydown()`
   * `mousetap()` / `mousedown()`
   * `mousex()` / `mousey()`
   * `scroll()`
   * `setmouse()`
   * `hidemouse()` / `showmouse()`

### 3 - License

# 1 - Building Libdraw

Libdraw currently supports 64-bit Linux and Windows (msys2/MinGW) targets. In order to compile Libdraw, you'll need the following:

 * GNU Make.

 * A C++ compiler supporting C++11 or higher.
   * The makefile defaults to `g++`, but you can change the `CXX` variable to the compiler of your choice.

 * OpenGL version 3.3 or higher.

 * [GLFW](https://www.glfw.org/) version 3 or higher.
   * The easiest way to do this is having GLFW installed as a shared library available somewhere on your PATH. You can also place
     a dynamic or static GLFW library in `lib/GLFW/`.
 
 * [SOIL](https://github.com/littlstar/soil), as a static library.
   * You can get SOIL from many package managers, or build it yourself from the linked repository. Once you have a copy, place a valid `libSOIL.a` in `lib/SOIL/`.

Once you have all the necessary dependencies, simply invoke the makefile. Run 
```
make libdraw.a
```

to build a static library, 

```
make libdraw.so
```

to build a shared object (Linux), or 
```
make libdraw.dll
```
to build a DLL (Windows).

# 2 - Documentation

## 2.1 - Window Management

```cpp
void window(int width, int height, const char* title)
```

Initializes Libdraw and creates a window with the provided dimensions and title. `window()` must be called prior to any other Libdraw function (so it's usually the first line of your program). Currently, `window()` cannot be called multiple times.

The width and height provided define the dimensions of the window's _internal surface_. All Libdraw rendering is done on this internal surface, and all positions are in terms of this surface's resolution. The window itself can be resized to any dimensions, and will render the internal surface scaled to the nearest integer scale that will fit within the window.

---

```cpp
bool running()
```

Returns true so long as the active window has not been closed. While this is `running()`'s nominal purpose, it's also used internally to mark the end of a frame - so you should call it at the end of each frame of your program.

The easiest way to do this is just to follow this rough pattern for creating Libdraw programs: 
```cpp
int main() {
    window(640, 480, "My Window");
    while (running()) {
        // do rendering here
    }
}
```

`running()` will also attempt to keep Libdraw running at exactly 60 frames per second. If your program is running faster than that, it will sleep the remaining time to maintain a relatively stable framerate.

---

```cpp
float seconds()
```

Returns the number of seconds the active window has been open for.

---

```cpp
int frames()
```

Returns the number of frames the active window has been open for.

---

## 2.2 - Images

```cpp
using Image = int
```

The `Image` type represents a handle to a two-dimensional array of 32-bit RGBA pixels. The actual memory of these images is managed internally by Libdraw.

---

```cpp
Image image(const char* path)
```

Loads an image from the provided file path. Supports all the image formats supported by the SOIL library: `.bmp`, `.png`, `.jpg`, `.tga`, `.dds`, `.psd`, and `.hdr`.

---

```cpp
Image newimage(int width, int height)
```

Creates a new image with the provided dimensions, filled with opaque white (`0xFFFFFF`) pixels.

---

```cpp
Image subimage(Image i, int x, int y, int width, int height)
```

Creates a new image from the rectangular subsection of image `i`, with top-left corner `(x, y)` and dimensions `width` and `height`. The resulting subimage does not have its own texture memory - it will always refer to a subsection of the original image. If that subsection of the original image is modified, the subimage will reflect those changes. 

`subimage()` _will_ create some persistent metadata for the new image, however. So, please cache the handles of subimages you create, otherwise this metadata will be effectively leaked.

---

```cpp
int width(Image i)
```

Returns the width of the provided image `i` in pixels.

---

```cpp
int height(Image i)
```

Returns the height of the provided image `i` in pixels.

---

```cpp
Image BLANK;
```

`BLANK` is a handle for an all-white (`0xFFFFFF`), opaque image. It's used internally in flat-color drawing operations such as `rect()` and `polygon()`, and is useful for drawing untextured geometry.

---

```cpp
Image SCREEN;
```

`SCREEN` is a handle for the internal surface of the active window. Its dimensions are the same as the `width` and `height` that the `window()` function was called with, and its contents are generally the pixels of the previous frame.

---

```cpp
struct Texture
```

The `Texture` type describes how an image should be applied to a piece of 3D geometry, like a cuboid.

---

```cpp
Texture nettex(Image i)
```

Returns a texture describing an image `i` that is meant to be interpreted as a net.

---

```cpp
Texture autotex(Image i)
```

Returns a texture describing an image `i` that is meant to be automatically tiled over a piece of geometry.

---

```cpp
Texture stretchtex(Image i)
```

Returns a texture describing an image `i` that is meant to be stretched over each face of a piece of geometry.

---

## 2.3 - Colors

```cpp
using Color = int
```

Represents a 32-bit RGBA color.

---

Libdraw includes some constants for common colors. These constants and their
corresponding RGBA values are shown in the table below.

|Constant|Hex|R,G,B,A|
|---|---|---|
| RED | `0xFF0000FF` | 255, 0, 0, 255 |
| ORANGE | `0xFF8000FF` | 255, 128, 0, 255 |
| YELLOW | `0xFFFF00FF` | 255, 255, 0, 255 |
| LIME | `0x80FF00FF` | 128, 255, 0, 255 |
| GREEN | `0x00FF00FF` | 0, 255, 0, 255 |
| CYAN | `0x00FFFFFF` | 0, 255, 255, 255 |
| BLUE | `0x0080FFFF` | 0, 128, 255, 255 |
| INDIGO | `0x0000FFFF` | 0, 0, 255, 255 |
| PURPLE | `0x8000FFFF` | 128, 0, 255, 255 |
| MAGENTA | `0xFF00FFFF` | 255, 0, 255, 255 |
| PINK | `0xFF80C0FF` | 255, 128, 192, 255 |
| BROWN | `0x804000FF` | 128, 64, 0, 255 | 
| WHITE | `0xFFFFFFFF` | 255, 255, 255, 255 |
| GRAY | `0x808080FF` | 128, 128, 128, 255 |
| BLACK | `0x000000FF` | 0, 0, 0, 255 |
| CLEAR | `0x00000000` | 0, 0, 0, 0 |

---

```cpp
Color rgba(int red, int green, int blue, int alpha)
Color rgb(int red, int green, int blue)
```

The `rgba()` function constructs a color from its individual color components. This function expects the provided components to be within the inclusive range [0, 255] - it may behave unpredictably if this is not the case.

The `rgb()` function is equivalent to calling `rgba()` with an alpha value of 255.

---

```cpp
void color(Color c)
```

Sets the drawing color to the provided one. This will cause all drawing operations to be tinted by this color until another call to `color()` changes it again. The color is set to white at the end of every frame.

---

## 2.4 - 2D Drawing

![2D Space](https://i.imgur.com/7LBUSVo.png)

All 2D drawing primitives take place in 2D space. In Libdraw, this space is defined by the X and Y axes. The origin (0, 0) is fixed at the top left corner of the screen. X values become more positive approaching the right edge of the screen, and Y values become more positive approaching the bottom of the screen.

```cpp
void rect(float x, float y, float width, float height)
```

Draws an untextured rectangle from (x, y) to (x + width, y + height). `rect()` is unaffected by changes to the drawing origin.

---

```cpp
void polygon(float x, float y, float radius, int sides)
void circle(float x, float y, float radius)
```

`polygon()` draws an untextured regular polygon with the provided radius and number of sides at the provided position.

`circle()` draws an approximate circle, equivalent to calling `polygon()` with 32 sides.

---

```cpp
void sprite(float x, float y, Image i)
```

Draws the provided image at the provided position.

---

```cpp
void font(Image i)
```

Interprets the provided image as a bitmap font and uses it as the font for subsequent text-drawing operations.

![Bitmap Font](https://i.imgur.com/3G4gBvt.png)

Libdraw's font format is pretty barebones. Bitmap font files are interpreted as 16 by 16 grids of characters. Given an 8-bit character, we can find its cell in the grid using its two 4-bit halves - the most significant 4 bits designate the row, and the least significant 4 bits designate the column.

Within each cell, Libdraw will compute a region called the "bounding box" - a rectangle centered in the grid cell with dimensions half those of the grid cell. This bounding box defines the height and width of the characters in the font, and most of the character should be drawn within. But, for certain characters (such as lowercase "g"), we might want parts of the character to extend beyond the normal character boundaries. The rest of the grid cell can be used for these kinds of additions.

---

```cpp
void text(float x, float y, const char* str)
void wraptext(float x, float y, const char* str, int width)
```

`text()` will draw the text string `str` at the provided position. Whitespace characters are accounted for by this function - single spaces advance the cursor
forward by one bounding box, tabs move the cursor forward to the nearest multiple of four characters, and line breaks move the cursor down and to the left to start a new line.

`wraptext()` functions for the most part like `text()`, but additionally takes a width parameter (in pixels). If the rendered text exceeds this width limit, Libdraw will break the line at the most recent whitespace character, and continue on the next line as if a line break had been encountered.

---

## 2.5 - Transformations

```cpp
enum Origin
```

![Origins](https://i.imgur.com/RHHou6C.png)

Most Libdraw drawing functions are defined in terms of a single point, not in terms of two or three points. The current origin defines how, relative to that point, the geometry will be drawn. If the origin is `TOP_LEFT` for instance, and we call `sprite(x, y, spr)`, the image `spr` will be drawn such that its top-left corner is at the point (x, y). If the origin was `CENTER`, it would be drawn centered on the point (x, y).

---

```cpp
enum Axis
```

Defines values denoting the X, Y, and Z axes.

---

```cpp
enum Direction
```

Defines values denoting all the axial directions, both positive and negative on each axis.

---

```cpp
enum Edge
```

![Edges](https://i.imgur.com/sWjZmE9.png)

Edge values denote the edges of an axis-aligned cube.

---

```cpp
void origin(Origin orig)
```

Sets the drawing origin to the provided one. Affects subsequent drawing calls until the drawing origin is changed again. The origin is set to `FRONT_TOP_LEFT` at the end of each frame.

---

```cpp
void rotate(float degrees, Axis axis)
```

Rotates subsequent geometry by the provided angle around the axis.

---

```cpp
void translate(float dx, float dy, float dz);
```

Translates subsequent geometry by the provided offsets in each axis.

---

```cpp
void scale(float factor);
void scale2d(float x, float y);
void scale3d(float x, float y, float z);
```

`scale()` scales subsequent geometry by the provided factor. Factors smaller than 1 will shrink things, factors larger than 1 will scale them up.

`scale2d()` will scale subsequent geometry by separate X and Y factors.

`scale3d()` will scale subsequent geometry by separate factors on each of the X, Y, and Z axes.

---

```cpp
void beginstate();
void endstate();
```

Transformations like `rotate`, `translate`, and `scale` are often tricky to use, since they will implicitly compound with each other, affect all subsequent draw calls, and introduce order dependence. Calling `beginstate()` before a sequence of these transformations will save the previous transformation state. Calling `endstate()` afterwards will restore the transformation state to the last saved one.

---

## 2.6 - 3D Drawing

```cpp
void cube(float x, float y, float z, float width, float height, float length, Texture tex)
```

Draws a 3D cube at the provided (x, y, z) position, with the provided width, length, and height.

---

```cpp
void board(float x, float y, float z, Image i)
```

Draws a 3D sprite at the provided (x, y, z) position, rotated so that it always faces the camera.

---

```cpp
void slant(float x, float y, float z, float width, float height, float length, Edge edge, Texture tex)
```

Draws a slanted triangular prism. Functions similarly to drawing a cube, but only draws the half of the cube nearest to the provided edge.

---

```cpp
void prism(float x, float y, float z, float width, float height, float length, Axis axis, int sides, Texture tex)
void cylinder(float x, float y, float z, float width, float height, float length, Axis axis, Texture tex)
```

`prism()` draws a prism at the provided (x, y, z) position, bounded by a cube with the provided width, length, and height. The ends of the prism are regular polygons with the provided number of sides, and the prism is oriented along the provided axis.

`cylinder()` draws a cylinder along an axis, equivalent to calling `prism()` with 32 sides.

---

```cpp
void pyramid(float x, float y, float z, float width, float height, float length, Direction dir, int sides, Texture tex)
void cone(float x, float y, float z, float width, float height, float length, Direction dir, Texture tex)
```

`pyramid()` draws a pyramid at the provided (x, y, z) position, bounded by a cube with the provided width, length, and height. The end of the pyramid is a regular polygon with the provided number of sides, and the pyramid points in the provided direction.

`cone()` draws a cone along an axis, equivalent to calling `pyramid()` with 32 sides.

---

```cpp
void hedron(float x, float y, float z, float width, float height, float length, int hsides, int vsides, Texture tex)
void sphere(float x, float y, float z, float width, float height, float length, Texture tex)
```

`sphere()` draws a sphere at the provided (x, y, z) position, bounded by a cube with the provided width, length, and height. `sphere()` is equivalent to calling `hedron()` with 16 hsides and 8 vsides.

`hedron()` essentially allows the drawing of different sphere variants. The given number of "hsides" is the number of longitude lines the solid will have, and the number of "vsides" is the number of latitude lines the solid will have. With a high number of hsides and vsides, the resulting solid is indistinguishable from a sphere. With lower numbers of sides, other kinds of solids can be formed.

---

## 2.7 - Camera Controls

```cpp
void ortho(float width, float height)
```

Changes the view to an orthographic projection, where the top-left corner of the screen is (0, 0), and the bottom-right corner of the screen is (width, height).

---

```cpp
void frustum(float width, float height, float fov)
```

Changes the view to a perspective projection, with an aspect ratio defined by the provided width and height, and the provided field-of-view angle (in degrees).

---

```cpp
void pan(float x, float y, float z)
```

Moves the camera by the provided X, Y, and Z offsets.

---

```cpp
void tilt(float degrees, Axis axis)
```

Rotates the camera by the provided angle along the provided axis.

---

```cpp
void snap(float x, float y, float z)
```

Sets the camera to the position (x, y, z).

---

```cpp
void look(float x, float y, float z, float yaw, float pitch)
void lookat(float x1, float y1, float z1, float x2, float y2, float z2)
```

`look()` moves the camera to the position (x, y, z), with the provided yaw and pitch angles (in degrees).

`lookat()` behaves similarly, but chooses the camera's yaw and pitch so that it is facing the point (x2, y2, z2).

---

## 2.8 - Models

```cpp
using Model = int
```

Models are handles to baked pieces of geometry. Without models, it's still possible to use all of Libdraw's rendering functionality. But it's much better for performance to assemble lots of geometry into a model and render it all at once. Similarly to images, Libdraw manages the storage for these models internally. Also similarly to images, models persist for the duration of your program - so you should try and reuse them whenever possible!

---

```cpp
Model createmodel()
```

Creates a new empty model and returns a handle to it.

---

```cpp 
Model sketch()
void sketchto(Model model)
```

`sketch()` is somewhat of a complicated function. It will take all of the draw calls currently in Libdraw's drawing queue, and instead of drawing them to the screen, draw them to a new model and return it.

`sketchto()` will also clear the draw queue to a model, but draws over the provided existing model instead of creating a new one.

---

```cpp
void flush()
```

This is a bit of a complicated function. `sketch()` can be a problematic function, since it's somewhat unpredictable when steps from the draw queue will be performed. If `sketch()` is called from within a long series of draw calls, the resulting model might end up with some unintended geometry. The `flush()` function can be used to explicitly flush the draw queue to the screen, so no existing steps are inadvertently captured in a model.

---

```cpp
void render(Model model, Image i)
```

Renders the provided model, using the provided image as a texture.

---

## 2.9 - Effects

```cpp
using Shader = int
```

A handle to a shader object.

---

```cpp
const char* DEFAULT_VSH
const char* DEFAULT_FSH
Shader DEFAULT_SHADER
```

`DEFAULT_VSH` and `DEFAULT_FSH` contain the GLSL source code for the default shader used by Libdraw. They're made accessible so that new shaders can be defined that make use of one of the existing default components.

`DEFAULT_SHADER` is a handle to the default shader used by Libdraw.

---

```cpp
Shader shader(const char* vsh, const char* fsh)
```

Compiles and returns a GLSL shader from the provided vertex and fragment shader sources.

---

```cpp
void paint(Image img)
void shade(Image img, Shader shader)
```

`paint()` will flush the draw queue, but instead of drawing to the screen, every step will draw to the provided image.

`shade()` behaves like `paint()`, but renders everything through the provided shader instead of the default one.

---

```cpp
void fog(Color color, float range)
void nofog()
```

`fog()` will set the fog state for subsequent geometry. The provided color, as you might expect, specifies the color of the fog. By default, the fog's density increases linearly with the distance from the camera, until the distance equals the specified range - at this point, the fog will become totally opaque.

`nofog()` disables fog for subsequent geometry, until a future call to `fog()` enables it again. `nofog()` is automatically called at the end of each frame.

---

## 2.10 - Input

```cpp
bool keytap(const char* key)
bool keydown(const char* key)
```

`keydown()` returns true if the given key is being pressed, false otherwise. 

`keytap()` returns true on the first frame the key is pressed, false otherwise.

---

```cpp
bool mousetap(MouseButton button)
bool mousedown(MouseButton button)
```

`mousedown()` returns true if the given mouse button is being pressed, false otherwise.

`mousetap()` returns true on the first frame the mouse button is pressed, false otherwise.

---

```cpp
float mousex()
float mousey()
```

`mousex()` returns the current X coordinate of the mouse, relative to the window surface.

`mousey()` returns the current Y coordinate of the mouse, relative to the window surface.

---

```cpp
float scroll()
```

Returns the amount the mouse wheel was vertically scrolled this frame.

---

```cpp
void setmouse(float x, float y)
```

Moves the mouse cursor to the point (x, y), relative to the window surface.

---

```cpp
void hidemouse()
void showmouse()
```

`hidemouse()` hides the mouse cursor.

`showmouse()` reveals the mouse cursor, if it was hidden.

# 3 - License

Libdraw is distributed under the [zlib/libpng license](https://opensource.org/licenses/Zlib).

```
Copyright (c) 2021 elucent

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
```

This repository also distributes headers for the GLFW and SOIL libraries. GLFW is distributed under the [zlib/libpng license](https://opensource.org/licenses/Zlib) license, and SOIL is distributed under the [MIT license](https://opensource.org/licenses/MIT). Their respective licenses are included with the source, at the `lib/GLFW/glfw3.h` and `lib/SOIL/SOIL.h` relative paths respectively.