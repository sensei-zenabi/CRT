# CRT

CRT emulator overlay built with SDL2 and OpenGL.

## Setup

On Debian/Ubuntu-based systems you can install the required development packages with:

```bash
./setup.sh
```

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
* On X11, the overlay captures the desktop each frame and feeds it into the shader chain so effects
  apply to the current screen contents. To avoid the overlay feeding back into its own capture, the
  window briefly hides itself from the compositor while grabbing the desktop, then restores
  opacity before drawing. Without X11, the overlay remains transparent with only shader-driven
  visuals visible.

Shaders are compiled as OpenGL fragment shaders. They receive common uniforms such as `Texture`,
`InputSize`, `TextureSize`, `OutputSize`, `FrameCount`, and `FrameDirection` for compatibility with
many libretro-style GLSL files. The overlay clears to transparent each frame so only shader output
is visible on top of the desktop while the captured desktop acts as the `Texture` input.

### Shader compatibility

All bundled shaders in `./shaders` are supported individually or chained:

* `fakelottes-geom.glsl` runs directly on the captured desktop buffer.
* `film_noise.glsl` automatically receives a procedurally generated repeating `noise1` texture for
  its grain and scratch sampling so it does not depend on external LUT assets.
* `vhs.glsl` receives a generated semi-transparent “play” overlay texture so its `play` sampler can
  display the expected on-screen-display without extra assets while still reading the captured
  desktop as the main `Texture` input.

Shaders are ping‑ponged through framebuffer objects in the order provided on the command line, so
mixing and matching the shipped GLSL files (or your own) will apply them sequentially on the live
desktop content.
