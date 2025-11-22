# CRT Shaderglass

This application renders a full-screen test pattern through a configurable GLSL shader pipeline using SDL2 and OpenGL. Multiple shaders can be supplied and are executed in order, allowing CRT-style effects to be layered together.

## Building

Use the provided Makefile. SDL2 and OpenGL development headers are required.

```bash
make
```

The resulting executable is named `crt`.

## Usage

Run with optional window size flags and one or more `--shader` arguments. When no shaders are provided, a built-in scanline shader is used.

```bash
./crt --width=1280 --height=720 \
  --shader shaders/fakelottes-geom.glsl \
  --shader shaders/film_noise.glsl
```

During execution, resize events automatically rebuild the framebuffer chain to match the new window size. Close the window to exit.
