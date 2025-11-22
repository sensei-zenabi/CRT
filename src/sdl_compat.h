#pragma once

#if __has_include(<SDL2/SDL.h>)
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#else

#include <cstddef>
#include <cstdint>

// Minimal SDL/OpenGL compatibility layer to allow building without the SDL2 development package.
using Uint32 = std::uint32_t;
using SDL_GLContext = void *;

using GLenum = unsigned int;
using GLuint = unsigned int;
using GLint = int;
using GLsizei = int;
using GLboolean = unsigned char;
using GLfloat = float;
using GLchar = char;
using GLsizeiptr = std::ptrdiff_t;

struct SDL_Window
{
};

constexpr std::uint32_t SDL_INIT_VIDEO = 0x00000020u;
constexpr std::uint32_t SDL_WINDOW_SHOWN = 0x00000004u;
constexpr int SDL_WINDOWPOS_CENTERED = 0;
constexpr std::uint32_t SDL_WINDOW_OPENGL = 0x00000002u;
constexpr std::uint32_t SDL_WINDOW_RESIZABLE = 0x00000020u;

enum SDL_GLattr
{
    SDL_GL_CONTEXT_MAJOR_VERSION,
    SDL_GL_CONTEXT_MINOR_VERSION,
    SDL_GL_CONTEXT_PROFILE_MASK,
    SDL_GL_DOUBLEBUFFER,
    SDL_GL_ALPHA_SIZE
};

constexpr int SDL_GL_CONTEXT_PROFILE_CORE = 1;

enum SDL_EventType
{
    SDL_QUIT = 0x100,
    SDL_WINDOWEVENT = 0x200
};

enum SDL_WindowEventID
{
    SDL_WINDOWEVENT_NONE,
    SDL_WINDOWEVENT_SIZE_CHANGED = 0x07
};

struct SDL_WindowEvent
{
    std::uint32_t type;
    std::uint32_t timestamp;
    std::uint32_t windowID;
    std::uint8_t event;
    int data1;
    int data2;
};

struct SDL_Event
{
    std::uint32_t type;
    SDL_WindowEvent window;
};

inline int SDL_Init(std::uint32_t)
{
    return 0;
}

inline void SDL_Quit() {}

inline const char *SDL_GetError()
{
    return "SDL2 development files not available";
}

inline SDL_Window *SDL_CreateWindow(const char *, int, int, int, int, std::uint32_t)
{
    return new SDL_Window();
}

inline void SDL_DestroyWindow(SDL_Window *window)
{
    delete window;
}

inline int SDL_GL_SetAttribute(SDL_GLattr, int)
{
    return 0;
}

inline SDL_GLContext SDL_GL_CreateContext(SDL_Window *)
{
    return reinterpret_cast<SDL_GLContext>(new int(0));
}

inline void SDL_GL_DeleteContext(SDL_GLContext context)
{
    delete reinterpret_cast<int *>(context);
}

inline int SDL_GL_SetSwapInterval(int)
{
    return 0;
}

inline void SDL_GL_SwapWindow(SDL_Window *) {}

inline void SDL_GetWindowPosition(SDL_Window *, int *x, int *y)
{
    if (x)
    {
        *x = 0;
    }
    if (y)
    {
        *y = 0;
    }
}

inline void SDL_GetWindowSize(SDL_Window *, int *w, int *h)
{
    if (w)
    {
        *w = 0;
    }
    if (h)
    {
        *h = 0;
    }
}

inline int SDL_PollEvent(SDL_Event *)
{
    return 0;
}

inline Uint32 SDL_GetWindowFlags(SDL_Window *)
{
    return SDL_WINDOW_SHOWN;
}

inline void SDL_HideWindow(SDL_Window *) {}

inline void SDL_ShowWindow(SDL_Window *) {}

constexpr GLenum GL_VERTEX_SHADER = 0x8B31;
constexpr GLenum GL_FRAGMENT_SHADER = 0x8B30;
constexpr GLenum GL_COMPILE_STATUS = 0x8B81;
constexpr GLenum GL_INFO_LOG_LENGTH = 0x8B84;
constexpr GLenum GL_LINK_STATUS = 0x8B82;
constexpr GLenum GL_ARRAY_BUFFER = 0x8892;
constexpr GLenum GL_STATIC_DRAW = 0x88E4;
constexpr GLenum GL_FLOAT = 0x1406;
constexpr GLboolean GL_FALSE = 0;
constexpr GLenum GL_TRIANGLES = 0x0004;
constexpr GLenum GL_TEXTURE0 = 0x84C0;
constexpr GLenum GL_TEXTURE1 = 0x84C1;
constexpr GLenum GL_TEXTURE_2D = 0x0DE1;
constexpr GLenum GL_RGBA8 = 0x8058;
constexpr GLenum GL_RGBA = 0x1908;
constexpr GLenum GL_UNSIGNED_BYTE = 0x1401;
constexpr GLenum GL_TEXTURE_MIN_FILTER = 0x2801;
constexpr GLenum GL_TEXTURE_MAG_FILTER = 0x2800;
constexpr GLenum GL_LINEAR = 0x2601;
constexpr GLenum GL_TEXTURE_WRAP_S = 0x2802;
constexpr GLenum GL_TEXTURE_WRAP_T = 0x2803;
constexpr GLenum GL_CLAMP_TO_EDGE = 0x812F;
constexpr GLenum GL_COLOR_ATTACHMENT0 = 0x8CE0;
constexpr GLenum GL_FRAMEBUFFER = 0x8D40;
constexpr GLenum GL_FRAMEBUFFER_COMPLETE = 0x8CD5;
constexpr GLenum GL_COLOR_BUFFER_BIT = 0x00004000;
constexpr GLenum GL_BLEND = 0x0BE2;
constexpr GLenum GL_SRC_ALPHA = 0x0302;
constexpr GLenum GL_ONE_MINUS_SRC_ALPHA = 0x0303;
constexpr GLboolean GL_TRUE = 1;

inline GLuint glCreateShader(GLenum)
{
    static GLuint counter = 1;
    return counter++;
}

inline void glShaderSource(GLuint, GLsizei, const GLchar *const *, const GLint *) {}

inline void glCompileShader(GLuint) {}

inline void glGetShaderiv(GLuint, GLenum, GLint *params)
{
    if (params)
    {
        *params = 1;
    }
}

inline void glGetShaderInfoLog(GLuint, GLsizei, GLsizei *, GLchar *) {}

inline void glDeleteShader(GLuint) {}

inline GLuint glCreateProgram()
{
    static GLuint counter = 100;
    return counter++;
}

inline void glAttachShader(GLuint, GLuint) {}

inline void glBindAttribLocation(GLuint, GLuint, const GLchar *) {}

inline void glLinkProgram(GLuint) {}

inline void glGetProgramiv(GLuint, GLenum, GLint *params)
{
    if (params)
    {
        *params = 1;
    }
}

inline void glGetProgramInfoLog(GLuint, GLsizei, GLsizei *, GLchar *) {}

inline void glDeleteProgram(GLuint) {}

inline GLint glGetUniformLocation(GLuint, const GLchar *)
{
    return -1;
}

inline void glUseProgram(GLuint) {}

inline void glUniform1i(GLint, GLint) {}

inline void glUniform1f(GLint, GLfloat) {}

inline void glUniform2f(GLint, GLfloat, GLfloat) {}

inline void glUniform4f(GLint, GLfloat, GLfloat, GLfloat, GLfloat) {}

inline void glUniformMatrix4fv(GLint, GLsizei, GLboolean, const GLfloat *) {}

inline void glGenVertexArrays(GLsizei n, GLuint *arrays)
{
    static GLuint counter = 200;
    for (GLsizei i = 0; i < n; ++i)
    {
        arrays[i] = counter++;
    }
}

inline void glGenBuffers(GLsizei n, GLuint *buffers)
{
    static GLuint counter = 300;
    for (GLsizei i = 0; i < n; ++i)
    {
        buffers[i] = counter++;
    }
}

inline void glBindVertexArray(GLuint) {}

inline void glBindBuffer(GLenum, GLuint) {}

inline void glBufferData(GLenum, GLsizeiptr, const void *, GLenum) {}

inline void glEnableVertexAttribArray(GLuint) {}

inline void glVertexAttribPointer(GLuint, GLint, GLenum, GLboolean, GLsizei, const void *) {}

inline void glBindFramebuffer(GLenum, GLuint) {}

inline GLenum glCheckFramebufferStatus(GLenum)
{
    return GL_FRAMEBUFFER_COMPLETE;
}

inline void glGenFramebuffers(GLsizei n, GLuint *framebuffers)
{
    static GLuint counter = 500;
    for (GLsizei i = 0; i < n; ++i)
    {
        framebuffers[i] = counter++;
    }
}

inline void glFramebufferTexture2D(GLenum, GLenum, GLenum, GLuint, GLint) {}

inline void glGenTextures(GLsizei n, GLuint *textures)
{
    static GLuint counter = 400;
    for (GLsizei i = 0; i < n; ++i)
    {
        textures[i] = counter++;
    }
}

inline void glBindTexture(GLenum, GLuint) {}

inline void glTexImage2D(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *) {}

inline void glTexSubImage2D(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void *) {}

inline void glTexParameteri(GLenum, GLenum, GLint) {}

inline void glReadBuffer(GLenum) {}

inline void glActiveTexture(GLenum) {}

inline void glCopyTexSubImage2D(GLenum, GLint, GLint, GLint, GLint, GLint, GLsizei, GLsizei) {}

inline void glDrawArrays(GLenum, GLint, GLsizei) {}

inline void glClearColor(GLfloat, GLfloat, GLfloat, GLfloat) {}

inline void glClear(GLenum) {}

inline void glEnable(GLenum) {}

inline void glBlendFunc(GLenum, GLenum) {}

inline void glViewport(GLint, GLint, GLsizei, GLsizei) {}

inline void glDeleteFramebuffers(GLsizei, const GLuint *) {}

inline void glDeleteTextures(GLsizei, const GLuint *) {}

inline void glDeleteVertexArrays(GLsizei, const GLuint *) {}

inline int SDL_SetWindowOpacity(SDL_Window *, float)
{
    return 0;
}

inline void SDL_SetHint(const char *, const char *) {}

constexpr const char *SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR = "SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR";

#endif
