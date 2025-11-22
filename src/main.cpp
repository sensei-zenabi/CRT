#define GL_GLEXT_PROTOTYPES 1

#include "sdl_compat.h"

#include <array>
#include <cstdint>
#include <exception>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    struct ShaderProgram
    {
        GLuint program = 0;
        GLint textureUniform = -1;
        GLint inputSizeUniform = -1;
        GLint textureSizeUniform = -1;
        GLint outputSizeUniform = -1;
        GLint frameCountUniform = -1;
        GLint frameDirectionUniform = -1;
        GLint mvpUniform = -1;
    };

    struct RenderTarget
    {
        GLuint framebuffer = 0;
        GLuint texture = 0;
    };

    struct Options
    {
        int width = 1280;
        int height = 720;
        float opacity = 0.8f;
        std::vector<std::string> shaderPaths;
    };

    constexpr std::string_view kDefaultShader = R"GLSL(
        #if defined(VERTEX)
        layout(location = 0) in vec4 VertexCoord;
        layout(location = 1) in vec2 TexCoord;
        out vec2 TEX0;
        uniform mat4 MVPMatrix;
        void main() {
            gl_Position = MVPMatrix * VertexCoord;
            TEX0 = TexCoord;
        }
        #elif defined(FRAGMENT)
        in vec2 TEX0;
        out vec4 FragColor;
        uniform sampler2D Texture;
        uniform vec2 InputSize;
        void main() {
            vec2 uv = TEX0;
            vec3 base = texture(Texture, uv).rgb;
            vec3 lines = vec3(sin(uv.y * InputSize.y * 3.14159));
            FragColor = vec4(base * (0.8 + 0.2 * lines), 0.75);
        }
        #endif
    )GLSL";

    void sdlCheck(bool success, const std::string &message)
    {
        if (!success)
        {
            throw std::runtime_error(message + ": " + SDL_GetError());
        }
    }

    std::string loadFile(const std::string &path)
    {
        std::ifstream stream(path, std::ios::in | std::ios::binary);
        if (!stream)
        {
            throw std::runtime_error("Failed to open shader: " + path);
        }

        std::ostringstream buffer;
        buffer << stream.rdbuf();
        return buffer.str();
    }

    GLuint compileShader(GLenum type, const std::string &source)
    {
        GLuint shader = glCreateShader(type);
        const char *raw = source.c_str();
        glShaderSource(shader, 1, &raw, nullptr);
        glCompileShader(shader);

        GLint status = GL_FALSE;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
        if (status != GL_TRUE)
        {
            GLint logLength = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
            std::string log(static_cast<size_t>(logLength), '\0');
            glGetShaderInfoLog(shader, logLength, nullptr, log.data());
            glDeleteShader(shader);
            throw std::runtime_error("Shader compile error: " + log);
        }

        return shader;
    }

    ShaderProgram buildShaderProgram(const std::string &source)
    {
        const std::string header = "#version 330 core\n";

        std::string vertexSource = header + "#define VERTEX\n" + source;
        std::string fragmentSource = header + "#define FRAGMENT\n" + source;

        GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
        GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);

        GLuint program = glCreateProgram();
        glAttachShader(program, vertexShader);
        glAttachShader(program, fragmentShader);
        glBindAttribLocation(program, 0, "VertexCoord");
        glBindAttribLocation(program, 1, "TexCoord");
        glBindAttribLocation(program, 2, "COLOR");
        glLinkProgram(program);

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        GLint linkStatus = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
        if (linkStatus != GL_TRUE)
        {
            GLint logLength = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
            std::string log(static_cast<size_t>(logLength), '\0');
            glGetProgramInfoLog(program, logLength, nullptr, log.data());
            glDeleteProgram(program);
            throw std::runtime_error("Program link error: " + log);
        }

        ShaderProgram wrapped;
        wrapped.program = program;
        wrapped.textureUniform = glGetUniformLocation(program, "Texture");
        wrapped.inputSizeUniform = glGetUniformLocation(program, "InputSize");
        wrapped.textureSizeUniform = glGetUniformLocation(program, "TextureSize");
        wrapped.outputSizeUniform = glGetUniformLocation(program, "OutputSize");
        wrapped.frameCountUniform = glGetUniformLocation(program, "FrameCount");
        wrapped.frameDirectionUniform = glGetUniformLocation(program, "FrameDirection");
        wrapped.mvpUniform = glGetUniformLocation(program, "MVPMatrix");

        return wrapped;
    }

    ShaderProgram buildShaderFromFile(const std::string &path)
    {
        const std::string fileSource = loadFile(path);
        return buildShaderProgram(fileSource);
    }

    GLuint buildFullscreenVAO()
    {
        std::array<float, 36> vertices = {
            // positions      // tex coords
            -1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f,
             1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 0.0f,
             1.0f,  1.0f, 0.0f, 1.0f, 1.0f, 1.0f,
            -1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f,
             1.0f,  1.0f, 0.0f, 1.0f, 1.0f, 1.0f,
            -1.0f,  1.0f, 0.0f, 1.0f, 0.0f, 1.0f,
        };

        GLuint vao = 0;
        GLuint vbo = 0;
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void *>(0));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void *>(4 * sizeof(float)));

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        return vao;
    }

    GLuint createTexture(int width, int height, const std::vector<std::uint8_t> &initialData)
    {
        GLuint texture = 0;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     initialData.empty() ? nullptr : initialData.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
        return texture;
    }

    RenderTarget createRenderTarget(int width, int height)
    {
        RenderTarget target;
        target.texture = createTexture(width, height, {});
        glGenFramebuffers(1, &target.framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, target.framebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target.texture, 0);

        const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE)
        {
            throw std::runtime_error("Framebuffer incomplete");
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return target;
    }

    std::vector<std::uint8_t> buildTestPattern(int width, int height)
    {
        std::vector<std::uint8_t> data(static_cast<size_t>(width * height * 4));
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                const float xf = static_cast<float>(x) / static_cast<float>(width);
                const float yf = static_cast<float>(y) / static_cast<float>(height);
                const size_t idx = static_cast<size_t>((y * width + x) * 4);
                data[idx + 0] = static_cast<std::uint8_t>(255.0f * xf);
                data[idx + 1] = static_cast<std::uint8_t>(255.0f * (1.0f - xf));
                data[idx + 2] = static_cast<std::uint8_t>(255.0f * yf);
                data[idx + 3] = 255;
            }
        }
        return data;
    }

    Options parseArgs(int argc, char **argv)
    {
        Options options;
        for (int i = 1; i < argc; ++i)
        {
            std::string arg = argv[i];
            if (arg == "--shader" && i + 1 < argc)
            {
                options.shaderPaths.emplace_back(argv[++i]);
            }
            else if (arg.rfind("--width=", 0) == 0)
            {
                options.width = std::stoi(arg.substr(8));
            }
            else if (arg.rfind("--height=", 0) == 0)
            {
                options.height = std::stoi(arg.substr(9));
            }
            else if (arg.rfind("--opacity=", 0) == 0)
            {
                options.opacity = std::stof(arg.substr(10));
                if (options.opacity < 0.0f)
                {
                    options.opacity = 0.0f;
                }
                else if (options.opacity > 1.0f)
                {
                    options.opacity = 1.0f;
                }
            }
            else
            {
                std::cerr << "Unrecognized argument: " << arg << "\n";
            }
        }
        return options;
    }

    std::vector<ShaderProgram> buildPipeline(const Options &options)
    {
        std::vector<ShaderProgram> pipeline;
        if (options.shaderPaths.empty())
        {
            pipeline.emplace_back(buildShaderProgram(std::string(kDefaultShader)));
            return pipeline;
        }

        pipeline.reserve(options.shaderPaths.size());
        for (const auto &path : options.shaderPaths)
        {
            pipeline.emplace_back(buildShaderFromFile(path));
        }
        return pipeline;
    }

    void setCommonUniforms(const ShaderProgram &program, int width, int height, int frameCount, int inputWidth, int inputHeight)
    {
        glUseProgram(program.program);
        if (program.textureUniform >= 0)
        {
            glUniform1i(program.textureUniform, 0);
        }
        if (program.inputSizeUniform >= 0)
        {
            glUniform2f(program.inputSizeUniform, static_cast<float>(inputWidth), static_cast<float>(inputHeight));
        }
        if (program.textureSizeUniform >= 0)
        {
            glUniform2f(program.textureSizeUniform, static_cast<float>(inputWidth), static_cast<float>(inputHeight));
        }
        if (program.outputSizeUniform >= 0)
        {
            glUniform2f(program.outputSizeUniform, static_cast<float>(width), static_cast<float>(height));
        }
        if (program.frameCountUniform >= 0)
        {
            glUniform1i(program.frameCountUniform, frameCount);
        }
        if (program.frameDirectionUniform >= 0)
        {
            glUniform1i(program.frameDirectionUniform, 1);
        }
        if (program.mvpUniform >= 0)
        {
            const std::array<float, 16> identity = {1.0f, 0.0f, 0.0f, 0.0f,
                                                    0.0f, 1.0f, 0.0f, 0.0f,
                                                    0.0f, 0.0f, 1.0f, 0.0f,
                                                    0.0f, 0.0f, 0.0f, 1.0f};
            glUniformMatrix4fv(program.mvpUniform, 1, GL_FALSE, identity.data());
        }
    }

    void renderPipeline(const std::vector<ShaderProgram> &pipeline,
                        const std::vector<RenderTarget> &targets,
                        GLuint vao,
                        GLuint baseTexture,
                        int width,
                        int height,
                        int frameCount)
    {
        GLuint inputTexture = baseTexture;
        int inputWidth = width;
        int inputHeight = height;

        for (size_t index = 0; index < pipeline.size(); ++index)
        {
            const bool isLast = index + 1 == pipeline.size();
            GLuint framebuffer = isLast ? 0 : targets[index % targets.size()].framebuffer;
            GLuint outputTexture = isLast ? 0 : targets[index % targets.size()].texture;

            glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
            glViewport(0, 0, width, height);
            glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, inputTexture);

            const ShaderProgram &program = pipeline[index];
            setCommonUniforms(program, width, height, frameCount, inputWidth, inputHeight);

            glBindVertexArray(vao);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindVertexArray(0);

            inputTexture = isLast ? baseTexture : outputTexture;
            inputWidth = width;
            inputHeight = height;
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
}

int main(int argc, char **argv)
{
    try
    {
        Options options = parseArgs(argc, argv);

        sdlCheck(SDL_Init(SDL_INIT_VIDEO) == 0, "SDL_Init failed");

        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

        SDL_Window *window = SDL_CreateWindow("Shaderglass CRT", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                              options.width, options.height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
        sdlCheck(window != nullptr, "SDL_CreateWindow failed");

        sdlCheck(SDL_SetWindowOpacity(window, options.opacity) == 0, "SDL_SetWindowOpacity failed");

        SDL_GLContext context = SDL_GL_CreateContext(window);
        sdlCheck(context != nullptr, "SDL_GL_CreateContext failed");

        SDL_GL_SetSwapInterval(1);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        std::vector<ShaderProgram> pipeline = buildPipeline(options);
        GLuint vao = buildFullscreenVAO();

        const int patternWidth = options.width;
        const int patternHeight = options.height;
        const auto pattern = buildTestPattern(patternWidth, patternHeight);
        GLuint baseTexture = createTexture(patternWidth, patternHeight, pattern);

        std::vector<RenderTarget> targets;
        for (size_t i = 0; i + 1 < pipeline.size(); ++i)
        {
            targets.emplace_back(createRenderTarget(options.width, options.height));
        }

        bool running = true;
        int frameCount = 0;
        while (running)
        {
            SDL_Event event;
            while (SDL_PollEvent(&event))
            {
                if (event.type == SDL_QUIT)
                {
                    running = false;
                }
                else if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                {
                    options.width = event.window.data1;
                    options.height = event.window.data2;
                    targets.clear();
                    for (size_t i = 0; i + 1 < pipeline.size(); ++i)
                    {
                        targets.emplace_back(createRenderTarget(options.width, options.height));
                    }
                }
            }

            renderPipeline(pipeline, targets, vao, baseTexture, options.width, options.height, frameCount);
            SDL_GL_SwapWindow(window);
            frameCount++;
        }

        for (const auto &target : targets)
        {
            glDeleteFramebuffers(1, &target.framebuffer);
            glDeleteTextures(1, &target.texture);
        }
        glDeleteTextures(1, &baseTexture);
        for (const auto &program : pipeline)
        {
            glDeleteProgram(program.program);
        }
        glDeleteVertexArrays(1, &vao);

        SDL_GL_DeleteContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Fatal error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
