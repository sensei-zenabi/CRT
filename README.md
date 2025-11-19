# CRT

CRT emulator overlay built with SDL2 and OpenGL.

## Building

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
