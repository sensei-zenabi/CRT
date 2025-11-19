#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES 1
#endif

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_syswm.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <random>
#include <vector>

#ifdef CRT_ENABLE_X11
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/extensions/shape.h>
#include <X11/Xutil.h>
#endif

namespace
{
struct GlPass
{
    GLuint program{0};
    GLint textureLocation{-1};
    GLint playTextureLocation{-1};
    GLint noiseTextureLocation{-1};
    GLint resolutionLocation{-1};
    GLint timeLocation{-1};
    GLint frameCountLocation{-1};
    GLint frameDirectionLocation{-1};
    GLint inputSizeLocation{-1};
    GLint outputSizeLocation{-1};
    GLint textureSizeLocation{-1};
};

struct FramebufferPair
{
    GLuint framebuffer{0};
    GLuint texture{0};
};

struct CaptureContext
{
    Display* display{nullptr};
    ::Window root{0};
};

constexpr std::string_view kUsage =
    "Usage: crt -s <shader.glsl> [-s <shader2.glsl> ...]\n";

[[nodiscard]] std::optional<std::string> read_file(const std::filesystem::path& path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        return std::nullopt;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

[[nodiscard]] std::string build_fragment_source(const std::string& source)
{
    std::istringstream input(source);
    std::string line;
    std::ostringstream stripped;
    bool firstLine = true;
    while (std::getline(input, line))
    {
        if (firstLine && line.rfind("#version", 0) == 0)
        {
            firstLine = false;
            continue;
        }
        firstLine = false;
        stripped << line << '\n';
    }

    std::ostringstream stream;
    stream << "#version 330 core\n";
    stream << "#define FRAGMENT 1\n";
    stream << stripped.str();
    return stream.str();
}

[[nodiscard]] GLuint compile_shader(GLenum type, const std::string& source)
{
    const GLuint shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_FALSE)
    {
        GLint logLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
        std::string log(static_cast<std::size_t>(logLength), '\0');
        glGetShaderInfoLog(shader, logLength, nullptr, log.data());
        std::cerr << "Shader compilation failed: " << log << '\n';
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

[[nodiscard]] GLuint create_program(const std::string& fragmentSource)
{
    const char* vertexSource = R"(
        #version 330 core
        layout(location = 0) in vec2 aPos;
        layout(location = 1) in vec2 aTexCoord;
        out vec4 TEX0;
        out vec4 COL0;
        void main()
        {
            TEX0 = vec4(aTexCoord, 0.0, 1.0);
            COL0 = vec4(1.0);
            gl_Position = vec4(aPos, 0.0, 1.0);
        }
    )";

    GLuint vertexShader = compile_shader(GL_VERTEX_SHADER, vertexSource);
    if (vertexShader == 0)
    {
        return 0;
    }

    GLuint fragmentShader = compile_shader(GL_FRAGMENT_SHADER, fragmentSource);
    if (fragmentShader == 0)
    {
        glDeleteShader(vertexShader);
        return 0;
    }

    const GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    GLint linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked == GL_FALSE)
    {
        GLint logLength = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
        std::string log(static_cast<std::size_t>(logLength), '\0');
        glGetProgramInfoLog(program, logLength, nullptr, log.data());
        std::cerr << "Program link failed: " << log << '\n';
        glDeleteProgram(program);
        return 0;
    }

    return program;
}

[[nodiscard]] std::optional<GlPass> load_pass_from_file(const std::filesystem::path& path)
{
    auto content = read_file(path);
    if (!content)
    {
        std::cerr << "Failed to read shader: " << path << '\n';
        return std::nullopt;
    }

    GlPass pass;
    pass.program = create_program(build_fragment_source(*content));
    if (pass.program == 0)
    {
        return std::nullopt;
    }

    pass.textureLocation = glGetUniformLocation(pass.program, "Texture");
    pass.playTextureLocation = glGetUniformLocation(pass.program, "play");
    pass.noiseTextureLocation = glGetUniformLocation(pass.program, "noise1");
    pass.resolutionLocation = glGetUniformLocation(pass.program, "uResolution");
    pass.timeLocation = glGetUniformLocation(pass.program, "uTime");
    pass.frameCountLocation = glGetUniformLocation(pass.program, "FrameCount");
    pass.frameDirectionLocation = glGetUniformLocation(pass.program, "FrameDirection");
    pass.inputSizeLocation = glGetUniformLocation(pass.program, "InputSize");
    pass.outputSizeLocation = glGetUniformLocation(pass.program, "OutputSize");
    pass.textureSizeLocation = glGetUniformLocation(pass.program, "TextureSize");

    return pass;
}

bool create_texture(GLuint& texture, int width, int height)
{
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    std::vector<unsigned char> transparent(static_cast<std::size_t>(width * height * 4u), 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 transparent.data());
    return glGetError() == GL_NO_ERROR;
}

bool create_framebuffer_pair(FramebufferPair& pair, int width, int height)
{
    glGenFramebuffers(1, &pair.framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, pair.framebuffer);

    if (!create_texture(pair.texture, width, height))
    {
        return false;
    }

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pair.texture, 0);

    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return status == GL_FRAMEBUFFER_COMPLETE;
}

bool create_noise_texture(GLuint& texture, int size)
{
    std::vector<unsigned char> noise(static_cast<std::size_t>(size * size * 4u));
    std::mt19937 rng(1337u);
    std::uniform_int_distribution<int> dist(0, 255);
    for (auto& value : noise)
    {
        value = static_cast<unsigned char>(dist(rng));
    }

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, noise.data());
    const GLenum error = glGetError();
    glBindTexture(GL_TEXTURE_2D, 0);
    return error == GL_NO_ERROR;
}

bool create_play_overlay_texture(GLuint& texture, int width, int height)
{
    std::vector<unsigned char> pixels(static_cast<std::size_t>(width * height * 4u), 0);

    const float ax = static_cast<float>(width) * 0.28f;
    const float ay = static_cast<float>(height) * 0.25f;
    const float bx = ax;
    const float by = static_cast<float>(height) * 0.75f;
    const float cx = static_cast<float>(width) * 0.78f;
    const float cy = static_cast<float>(height) * 0.50f;

    auto sign = [](float px, float py, float x1, float y1, float x2, float y2) {
        return (px - x2) * (y1 - y2) - (x1 - x2) * (py - y2);
    };

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            const float px = static_cast<float>(x) + 0.5f;
            const float py = static_cast<float>(y) + 0.5f;

            const bool b1 = sign(px, py, ax, ay, bx, by) < 0.0f;
            const bool b2 = sign(px, py, bx, by, cx, cy) < 0.0f;
            const bool b3 = sign(px, py, cx, cy, ax, ay) < 0.0f;

            const bool inside = (b1 == b2) && (b2 == b3);
            if (inside)
            {
                const std::size_t idx = static_cast<std::size_t>((y * width + x) * 4);
                pixels[idx + 0] = 0xFF;
                pixels[idx + 1] = 0xFF;
                pixels[idx + 2] = 0xFF;
                pixels[idx + 3] = 0xD0;
            }
        }
    }

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 pixels.data());
    const GLenum error = glGetError();
    glBindTexture(GL_TEXTURE_2D, 0);
    return error == GL_NO_ERROR;
}

void destroy_framebuffer_pair(FramebufferPair& pair)
{
    if (pair.texture != 0)
    {
        glDeleteTextures(1, &pair.texture);
        pair.texture = 0;
    }
    if (pair.framebuffer != 0)
    {
        glDeleteFramebuffers(1, &pair.framebuffer);
        pair.framebuffer = 0;
    }
}

[[nodiscard]] bool configure_input_passthrough(SDL_Window* window)
{
#ifdef CRT_ENABLE_X11
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (SDL_GetWindowWMInfo(window, &info) == SDL_TRUE && info.subsystem == SDL_SYSWM_X11)
    {
        Display* display = info.info.x11.display;
        ::Window xwindow = info.info.x11.window;

        int shapeEvent = 0;
        int shapeError = 0;
        if (XShapeQueryExtension(display, &shapeEvent, &shapeError))
        {
            XRectangle rectangle{0, 0, 0, 0};
            XShapeCombineRectangles(display, xwindow, ShapeInput, 0, 0, &rectangle, 1, ShapeSet,
                                    Unsorted);
        }

        Atom wmState = XInternAtom(display, "_NET_WM_STATE", False);
        Atom wmStateAbove = XInternAtom(display, "_NET_WM_STATE_ABOVE", False);
        if (wmState != None && wmStateAbove != None)
        {
            const Atom atoms[1] = {wmStateAbove};
            XChangeProperty(display, xwindow, wmState, XA_ATOM, 32, PropModeReplace,
                            reinterpret_cast<const unsigned char*>(atoms), 1);
        }

        XFlush(display);
    }
#endif
    return true;
}

[[nodiscard]] std::optional<CaptureContext> get_capture_context(SDL_Window* window)
{
#ifdef CRT_ENABLE_X11
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (SDL_GetWindowWMInfo(window, &info) == SDL_TRUE && info.subsystem == SDL_SYSWM_X11)
    {
        CaptureContext context{};
        context.display = info.info.x11.display;
        context.root = DefaultRootWindow(context.display);
        return context;
    }
#endif
    return std::nullopt;
}

[[nodiscard]] bool capture_root_to_texture(const CaptureContext& context, int width, int height,
                                           GLuint texture)
{
#ifdef CRT_ENABLE_X11
    if (context.display == nullptr || context.root == 0)
    {
        return false;
    }

    XImage* image = XGetImage(context.display, context.root, 0, 0, static_cast<unsigned int>(width),
                              static_cast<unsigned int>(height), AllPlanes, ZPixmap);
    if (image == nullptr)
    {
        return false;
    }

    std::vector<unsigned char> converted;
    GLenum format = GL_BGRA;
    GLenum type = GL_UNSIGNED_BYTE;

    if (image->bits_per_pixel == 24)
    {
        converted.resize(static_cast<std::size_t>(width * height * 4));
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                const char* src = image->data + y * image->bytes_per_line + x * 3;
                const std::size_t dstIndex = static_cast<std::size_t>((y * width + x) * 4);
                converted[dstIndex + 0] = static_cast<unsigned char>(src[0]);
                converted[dstIndex + 1] = static_cast<unsigned char>(src[1]);
                converted[dstIndex + 2] = static_cast<unsigned char>(src[2]);
                converted[dstIndex + 3] = 0xFF;
            }
        }
        format = GL_BGRA;
        type = GL_UNSIGNED_BYTE;
    }
    else if (image->bits_per_pixel == 32)
    {
        // 32-bit XImage is typically BGRA or ARGB; using BGRA matches the default masks.
        format = GL_BGRA;
        type = GL_UNSIGNED_BYTE;
    }
    else
    {
        XDestroyImage(image);
        return false;
    }

    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    const void* pixels = converted.empty() ? static_cast<const void*>(image->data)
                                           : static_cast<const void*>(converted.data());
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, format, type, pixels);
    const GLenum error = glGetError();
    XDestroyImage(image);
    return error == GL_NO_ERROR;
#else
    (void)context;
    (void)width;
    (void)height;
    (void)texture;
    return false;
#endif
}

[[nodiscard]] bool parse_arguments(int argc, char** argv, std::vector<std::filesystem::path>& shaders)
{
    for (int i = 1; i < argc; ++i)
    {
        const std::string_view arg(argv[i]);
        if (arg == "-s" || arg == "--shader")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "Missing shader path after " << arg << '\n';
                return false;
            }
            shaders.emplace_back(argv[++i]);
        }
        else
        {
            std::cerr << "Unknown argument: " << arg << '\n';
            return false;
        }
    }

    if (shaders.empty())
    {
        std::cerr << kUsage;
        return false;
    }

    return true;
}

bool recreate_buffers_if_needed(int width, int height, FramebufferPair& a, FramebufferPair& b,
                                GLuint& captureTexture)
{
    static int currentWidth = 0;
    static int currentHeight = 0;

    if (width == currentWidth && height == currentHeight && a.texture != 0 && b.texture != 0 &&
        captureTexture != 0)
    {
        return true;
    }

    destroy_framebuffer_pair(a);
    destroy_framebuffer_pair(b);
    if (captureTexture != 0)
    {
        glDeleteTextures(1, &captureTexture);
        captureTexture = 0;
    }

    if (!create_framebuffer_pair(a, width, height) || !create_framebuffer_pair(b, width, height))
    {
        std::cerr << "Failed to create framebuffers" << '\n';
        return false;
    }

    if (!create_texture(captureTexture, width, height))
    {
        std::cerr << "Failed to create capture texture" << '\n';
        return false;
    }

    currentWidth = width;
    currentHeight = height;
    return true;
}

void set_common_uniforms(const GlPass& pass, int width, int height, float timeSeconds,
                         int frameCount)
{
    if (pass.resolutionLocation >= 0)
    {
        glUniform2f(pass.resolutionLocation, static_cast<float>(width), static_cast<float>(height));
    }
    if (pass.timeLocation >= 0)
    {
        glUniform1f(pass.timeLocation, timeSeconds);
    }
    if (pass.frameCountLocation >= 0)
    {
        glUniform1i(pass.frameCountLocation, frameCount);
    }
    if (pass.frameDirectionLocation >= 0)
    {
        glUniform1i(pass.frameDirectionLocation, 1);
    }
    if (pass.inputSizeLocation >= 0)
    {
        glUniform2f(pass.inputSizeLocation, static_cast<float>(width), static_cast<float>(height));
    }
    if (pass.outputSizeLocation >= 0)
    {
        glUniform2f(pass.outputSizeLocation, static_cast<float>(width), static_cast<float>(height));
    }
    if (pass.textureSizeLocation >= 0)
    {
        glUniform2f(pass.textureSizeLocation, static_cast<float>(width), static_cast<float>(height));
    }
}

void render_pass(const GlPass& pass, GLuint inputTexture, int width, int height, float timeSeconds,
                 int frameCount, GLuint noiseTexture, GLuint playTexture,
                 const FramebufferPair* target)
{
    if (target != nullptr)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, target->framebuffer);
    }
    else
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    glViewport(0, 0, width, height);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(pass.program);
    set_common_uniforms(pass, width, height, timeSeconds, frameCount);

    GLint textureUnit = 0;
    if (pass.textureLocation >= 0)
    {
        glUniform1i(pass.textureLocation, textureUnit);
        glActiveTexture(GL_TEXTURE0 + textureUnit);
        glBindTexture(GL_TEXTURE_2D, inputTexture);
        ++textureUnit;
    }
    if (pass.playTextureLocation >= 0)
    {
        glUniform1i(pass.playTextureLocation, textureUnit);
        glActiveTexture(GL_TEXTURE0 + textureUnit);
        const GLuint overlay = playTexture != 0 ? playTexture : inputTexture;
        glBindTexture(GL_TEXTURE_2D, overlay);
        ++textureUnit;
    }
    if (pass.noiseTextureLocation >= 0 && noiseTexture != 0)
    {
        glUniform1i(pass.noiseTextureLocation, textureUnit);
        glActiveTexture(GL_TEXTURE0 + textureUnit);
        glBindTexture(GL_TEXTURE_2D, noiseTexture);
        ++textureUnit;
    }

    glDrawArrays(GL_TRIANGLES, 0, 6);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void setup_quad(GLuint& vao, GLuint& vbo)
{
    constexpr float vertices[] = {
        // positions   // tex coords
        -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f, 0.0f, -1.0f, 1.0f, 0.0f, 1.0f,
        -1.0f, 1.0f, 0.0f, 1.0f, 1.0f, -1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f};

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(0));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          reinterpret_cast<void*>(2 * sizeof(float)));

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<std::filesystem::path> shaderPaths;
    if (!parse_arguments(argc, argv, shaderPaths))
    {
        return 1;
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        std::cerr << "SDL init failed: " << SDL_GetError() << '\n';
        return 1;
    }

    SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

    SDL_DisplayMode displayMode;
    if (SDL_GetDesktopDisplayMode(0, &displayMode) != 0)
    {
        std::cerr << "Failed to query display mode: " << SDL_GetError() << '\n';
        SDL_Quit();
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("CRT", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          displayMode.w, displayMode.h,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_BORDERLESS |
                                              SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_ALWAYS_ON_TOP);

    if (window == nullptr)
    {
        std::cerr << "Failed to create window: " << SDL_GetError() << '\n';
        SDL_Quit();
        return 1;
    }

    SDL_GLContext context = SDL_GL_CreateContext(window);
    if (context == nullptr)
    {
        std::cerr << "Failed to create GL context: " << SDL_GetError() << '\n';
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_GL_SetSwapInterval(1);

    if (!configure_input_passthrough(window))
    {
        std::cerr << "Warning: failed to configure input pass-through" << '\n';
    }

    const auto captureContext = get_capture_context(window);
    if (!captureContext)
    {
        std::cerr << "Warning: X11 capture unavailable; overlay will be transparent" << '\n';
    }

    std::vector<GlPass> passes;
    passes.reserve(shaderPaths.size());
    for (const auto& path : shaderPaths)
    {
        auto pass = load_pass_from_file(path);
        if (!pass)
        {
            SDL_GL_DeleteContext(context);
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 1;
        }
        passes.push_back(*pass);
    }

    GLuint vao = 0;
    GLuint vbo = 0;
    setup_quad(vao, vbo);

    FramebufferPair framebufferA{};
    FramebufferPair framebufferB{};
    GLuint captureTexture = 0;
    GLuint noiseTexture = 0;
    GLuint playTexture = 0;

    int width = 0;
    int height = 0;
    SDL_GetWindowSize(window, &width, &height);
    if (!recreate_buffers_if_needed(width, height, framebufferA, framebufferB, captureTexture))
    {
        SDL_GL_DeleteContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    if (!create_noise_texture(noiseTexture, 256))
    {
        std::cerr << "Failed to create noise texture" << '\n';
        SDL_GL_DeleteContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    if (!create_play_overlay_texture(playTexture, 192, 96))
    {
        std::cerr << "Failed to create play overlay texture" << '\n';
        SDL_GL_DeleteContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindVertexArray(vao);

    bool running = true;
    auto startTime = std::chrono::steady_clock::now();
    int frameCount = 0;

    bool captureWarningPrinted = false;

    while (running)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event) != 0)
        {
            if (event.type == SDL_QUIT)
            {
                running = false;
            }
            else if (event.type == SDL_KEYDOWN && event.key.keysym.scancode == SDL_SCANCODE_F12)
            {
                running = false;
            }
            else if (event.type == SDL_WINDOWEVENT &&
                     (event.window.event == SDL_WINDOWEVENT_RESIZED ||
                      event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED))
            {
                width = event.window.data1;
                height = event.window.data2;
                recreate_buffers_if_needed(width, height, framebufferA, framebufferB, captureTexture);
            }
        }

        auto now = std::chrono::steady_clock::now();
        const float elapsedSeconds =
            std::chrono::duration_cast<std::chrono::duration<float>>(now - startTime).count();

        FramebufferPair* writeTarget = &framebufferA;
        FramebufferPair* readTarget = &framebufferB;
        GLuint inputTexture = captureTexture;

        if (captureContext)
        {
            const bool captured = capture_root_to_texture(*captureContext, width, height, captureTexture);
            if (!captured && !captureWarningPrinted)
            {
                std::cerr << "Warning: failed to capture root window; using last successful frame if available." << '\n';
                captureWarningPrinted = true;
            }
        }

        for (std::size_t i = 0; i < passes.size(); ++i)
        {
            const bool lastPass = (i + 1 == passes.size());

            if (lastPass)
            {
                render_pass(passes[i], inputTexture, width, height, elapsedSeconds, frameCount,
                            noiseTexture, playTexture,
                            nullptr);
            }
            else
            {
                render_pass(passes[i], inputTexture, width, height, elapsedSeconds, frameCount,
                            noiseTexture, playTexture,
                            writeTarget);
                std::swap(writeTarget, readTarget);
                inputTexture = readTarget->texture;
            }
        }

        SDL_GL_SwapWindow(window);
        ++frameCount;
    }

    destroy_framebuffer_pair(framebufferA);
    destroy_framebuffer_pair(framebufferB);

    if (captureTexture != 0)
    {
        glDeleteTextures(1, &captureTexture);
    }
    if (noiseTexture != 0)
    {
        glDeleteTextures(1, &noiseTexture);
    }
    if (playTexture != 0)
    {
        glDeleteTextures(1, &playTexture);
    }

    if (vbo != 0)
    {
        glDeleteBuffers(1, &vbo);
    }
    if (vao != 0)
    {
        glDeleteVertexArrays(1, &vao);
    }

    for (const auto& pass : passes)
    {
        if (pass.program != 0)
        {
            glDeleteProgram(pass.program);
        }
    }

    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
