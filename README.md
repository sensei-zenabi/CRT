# CRT

CRT emulator overlay built with SDL2 and OpenGL.

## Building

You can build either with the provided Makefile or with CMake:

### Make

```bash
make
```

The Makefile uses `pkg-config` to locate SDL2, OpenGL (or `-lGL` as a fallback), and
optionally X11 for input passthrough. The resulting binary is `./crt` in the repository root.
Install the development packages for SDL2, OpenGL, and X11 (e.g., `libsdl2-dev`,
`libgl1-mesa-dev`, `libx11-dev`, `libxext-dev`) so `pkg-config` can find them.

### CMake

```bash
cmake -S . -B build
cmake --build build
```

## Usage

Launch the transparent fullscreen overlay with one or more shader files:

```bash
./build/crt -s shaders/fakelottes-geom.glsl -s shaders/vhs.glsl
```

* The window is borderless, fullscreen, and always on top.
* Keyboard, mouse, and other input are passed through to the desktop beneath.
* Press **F12** to exit.

Shaders are compiled as OpenGL fragment shaders. They receive common uniforms such as `Texture`,
`InputSize`, `TextureSize`, `OutputSize`, `FrameCount`, and `FrameDirection` for compatibility with
many libretro-style GLSL files. The overlay clears to transparent each frame so only shader output
is visible on top of the desktop.
