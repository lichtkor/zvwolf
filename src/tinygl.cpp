#include "glad.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "tinygl.h"
#include <GLFW/glfw3.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <fstream>
#include <iostream>
#include <thread>
#include <unordered_map>

namespace TinyGL
{

int width = 0;
int height = 0;

GLFWwindow* window = nullptr;

struct Vertex
{
    float x;
    float y;
    float r;
    float g;
    float b;
    float a;
};

std::vector<Vertex> vertices;
std::unordered_map<int, bool> keyStates;

int windowWidth = 0;
int windowHeight = 0;
int windowedPosX = 0;
int windowedPosY = 0;

bool isFullscreen = false;
bool mouseLocked = false;
double lastMouseX = 0.0;
double lastMouseY = 0.0;

GLuint primitiveVAO = 0;
GLuint primitiveVBO = 0;
Shader primitiveShader;
GLuint textureVAO = 0;
GLuint textureVBO = 0;
GLuint screenTexture = 0;
Shader textureShader;

bool rendererReady = false;
bool hasBuffer = false;
int textureWidth = 0;
int textureHeight = 0;

float clearR = 0.0f;
float clearG = 0.0f;
float clearB = 0.0f;

std::vector<uint8> inputCharacters;

inline uint32 packColor(const ColorRGB& color)
{
    return (0xFFu << 24) | (uint32(clampColor(color.b)) << 16) | (uint32(clampColor(color.g)) << 8) | uint32(clampColor(color.r));
}

inline ColorRGB unpackColor(uint32 color)
{
    return ColorRGB(uint8(color & 0xFFu), uint8((color >> 8) & 0xFFu), uint8((color >> 16) & 0xFFu));
}

inline void addVertex(float x, float y, const ColorRGB& color, float alpha = 1.0f)
{
    vertices.push_back({x, y, clampColor(color.r) / 255.0f, clampColor(color.g) / 255.0f, clampColor(color.b) / 255.0f, alpha});
}

void addQuad(float x1, float y1, float x2, float y2, const ColorRGB& color)
{
    addVertex(x1, y1, color);
    addVertex(x2, y1, color);
    addVertex(x2, y2, color);

    addVertex(x1, y1, color);
    addVertex(x2, y2, color);
    addVertex(x1, y2, color);
}

void addLineQuad(float x1, float y1, float x2, float y2, const ColorRGB& color)
{
    float dx = x2 - x1;
    float dy = y2 - y1;
    float length = std::sqrt(dx * dx + dy * dy);

    if(length <= 0.0f)
    {
        addQuad(x1, y1, x1 + 1.0f, y1 + 1.0f, color);
        return;
    }

    float px = -dy / length * 0.5f;
    float py = dx / length * 0.5f;

    addVertex(x1 + px, y1 + py, color);
    addVertex(x2 + px, y2 + py, color);
    addVertex(x2 - px, y2 - py, color);

    addVertex(x1 + px, y1 + py, color);
    addVertex(x2 - px, y2 - py, color);
    addVertex(x1 - px, y1 - py, color);
}

GLuint compileShader(GLenum type, const std::string& source)
{
    const char* text = source.c_str();
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &text, nullptr);
    glCompileShader(shader);

    GLint success = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    if(success != GL_TRUE)
    {
        char log[2048] = {};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::cerr << "TinyGL shader compilation failed:\n" << log << '\n';
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

GLuint createProgram(const std::string& vertexSource, const std::string& fragmentSource)
{
    GLuint vertex = compileShader(GL_VERTEX_SHADER, vertexSource);
    GLuint fragment = compileShader(GL_FRAGMENT_SHADER, fragmentSource);

    if(!vertex || !fragment)
    {
        if(vertex) glDeleteShader(vertex);
        if(fragment) glDeleteShader(fragment);
        return 0;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);

    GLint success = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &success);

    if(success != GL_TRUE)
    {
        char log[2048] = {};
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        std::cerr << "TinyGL shader linking failed:\n" << log << '\n';
        glDeleteProgram(program);
        program = 0;
    }

    glDeleteShader(vertex);
    glDeleteShader(fragment);
    return program;
}

void flushPrimitives()
{
    if(!rendererReady || vertices.empty())
        return;

    primitiveShader.use();
    primitiveShader.setVec2("uSize", float(width), float(height));

    glBindVertexArray(primitiveVAO);
    glBindBuffer(GL_ARRAY_BUFFER, primitiveVBO);
    glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(vertices.size() * sizeof(Vertex)), vertices.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, GLsizei(vertices.size()));
    vertices.clear();
}

void drawTexture()
{
    if(!rendererReady || !hasBuffer || !screenTexture)
        return;

    const float quad[] = {
        -1.0f,  1.0f, 0.0f, 1.0f,
         1.0f,  1.0f, 1.0f, 1.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
        -1.0f,  1.0f, 0.0f, 1.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f, 0.0f
    };

    textureShader.use();
    textureShader.setInt("uTexture", 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, screenTexture);
    glBindVertexArray(textureVAO);
    glBindBuffer(GL_ARRAY_BUFFER, textureVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

const char* primitiveVertexSource = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aColor;
uniform vec2 uSize;
out vec4 vColor;
void main()
{
    gl_Position = vec4(aPos.x / uSize.x * 2.0 - 1.0, 1.0 - aPos.y / uSize.y * 2.0, 0.0, 1.0);
    vColor = aColor;
}
)";

const char* primitiveFragmentSource = R"(#version 330 core
in vec4 vColor;
out vec4 fragColor;
void main()
{
    fragColor = vColor;
}
)";

const char* textureVertexSource = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
out vec2 vUV;
void main()
{
    gl_Position = vec4(aPos, 0.0, 1.0);
    vUV = vec2(aUV.x, 1.0 - aUV.y);
}
)";

const char* textureFragmentSource = R"(#version 330 core
in vec2 vUV;
out vec4 fragColor;
uniform sampler2D uTexture;
void main()
{
    fragColor = vec4(texture(uTexture, vUV).rgb, 1.0);
}
)";

void initRenderer()
{
    if(!primitiveShader.loadSource(primitiveVertexSource, primitiveFragmentSource))
        return;

    if(!textureShader.loadSource(textureVertexSource, textureFragmentSource))
        return;

    glGenVertexArrays(1, &primitiveVAO);
    glGenBuffers(1, &primitiveVBO);

    glBindVertexArray(primitiveVAO);
    glBindBuffer(GL_ARRAY_BUFFER, primitiveVBO);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_STREAM_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(sizeof(float) * 2));
    glEnableVertexAttribArray(1);

    glGenVertexArrays(1, &textureVAO);
    glGenBuffers(1, &textureVBO);

    glBindVertexArray(textureVAO);
    glBindBuffer(GL_ARRAY_BUFFER, textureVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 24, nullptr, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, reinterpret_cast<void*>(sizeof(float) * 2));
    glEnableVertexAttribArray(1);

    glGenTextures(1, &screenTexture);
    glBindTexture(GL_TEXTURE_2D, screenTexture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    vertices.reserve(65536);
    rendererReady = true;
}

void destroyRenderer()
{
    if(screenTexture) glDeleteTextures(1, &screenTexture);
    if(textureVBO) glDeleteBuffers(1, &textureVBO);
    if(textureVAO) glDeleteVertexArrays(1, &textureVAO);
    if(primitiveVBO) glDeleteBuffers(1, &primitiveVBO);
    if(primitiveVAO) glDeleteVertexArrays(1, &primitiveVAO);

    screenTexture = 0;
    textureVBO = 0;
    textureVAO = 0;
    primitiveVBO = 0;
    primitiveVAO = 0;

    textureShader.destroy();
    primitiveShader.destroy();
    rendererReady = false;
}

void resizeTexture(int w, int h)
{
    if(w <= 0 || h <= 0)
        return;

    glBindTexture(GL_TEXTURE_2D, screenTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    textureWidth = w;
    textureHeight = h;
}

void charCallback(GLFWwindow*, unsigned int codepoint)
{
    if(codepoint >= 32 && codepoint <= 126)
        inputCharacters.push_back(uint8(codepoint));
}

void keyCallback(GLFWwindow*, int key, int, int action, int)
{
    if(action != GLFW_PRESS && action != GLFW_REPEAT)
        return;

    if(key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER)
        inputCharacters.push_back(13);
    else if(key == GLFW_KEY_BACKSPACE)
        inputCharacters.push_back(8);
}

const unsigned char fontData[96][7] =
{
    {0,0,0,0,0,0,0},
    {4,4,4,4,4,0,4},
    {10,10,0,0,0,0,0},
    {10,31,10,31,10,0,0},
    {14,20,12,5,30,0,0},
    {25,25,2,4,19,19,0},
    {12,18,20,10,21,0,0},
    {4,4,0,0,0,0,0},
    {2,4,8,8,8,4,2},
    {8,4,2,2,2,4,8},
    {0,4,21,14,21,4,0},
    {0,4,4,31,4,4,0},
    {0,0,0,0,4,4,8},
    {0,0,0,31,0,0,0},
    {0,0,0,0,0,12,12},
    {1,2,4,8,16,0,0},
    {14,17,19,21,25,17,14},
    {4,12,4,4,4,4,14},
    {14,17,1,2,4,8,31},
    {30,1,1,14,1,17,14},
    {2,6,10,18,31,2,2},
    {31,16,30,1,1,17,14},
    {6,8,16,30,17,17,14},
    {31,1,2,4,8,8,8},
    {14,17,17,14,17,17,14},
    {14,17,17,15,1,2,12},
    {0,12,12,0,12,12,0},
    {0,12,12,0,12,12,8},
    {2,4,8,16,8,4,2},
    {0,0,31,0,31,0,0},
    {8,4,2,1,2,4,8},
    {14,17,1,2,4,0,4},
    {14,17,1,13,21,21,14},
    {14,17,17,31,17,17,17},
    {30,17,17,30,17,17,30},
    {14,17,16,16,16,17,14},
    {30,17,17,17,17,17,30},
    {31,16,16,30,16,16,31},
    {31,16,16,30,16,16,16},
    {14,17,16,23,17,17,14},
    {17,17,17,31,17,17,17},
    {14,4,4,4,4,4,14},
    {7,2,2,2,18,18,12},
    {17,18,20,24,20,18,17},
    {16,16,16,16,16,16,31},
    {17,27,21,17,17,17,17},
    {17,25,21,19,17,17,17},
    {14,17,17,17,17,17,14},
    {30,17,17,30,16,16,16},
    {14,17,17,17,21,18,13},
    {30,17,17,30,20,18,17},
    {15,16,16,14,1,1,30},
    {31,4,4,4,4,4,4},
    {17,17,17,17,17,17,14},
    {17,17,17,17,17,10,4},
    {17,17,17,21,21,21,10},
    {17,17,10,4,10,17,17},
    {17,17,10,4,4,4,4},
    {31,1,2,4,8,16,31},
    {14,8,8,8,8,8,14},
    {16,8,4,2,1,0,0},
    {14,2,2,2,2,2,14},
    {4,10,17,0,0,0,0},
    {0,0,0,0,0,0,31},
    {8,4,2,0,0,0,0},
    {0,0,14,1,15,17,15},
    {16,16,22,25,17,17,30},
    {0,0,14,17,16,17,14},
    {1,1,13,19,17,17,15},
    {0,0,14,17,31,16,14},
    {6,9,8,28,8,8,8},
    {0,0,15,17,15,1,14},
    {16,16,22,25,17,17,17},
    {4,0,12,4,4,4,14},
    {2,0,6,2,2,18,12},
    {16,18,20,24,20,18,17},
    {12,4,4,4,4,4,14},
    {0,0,26,21,21,21,21},
    {0,0,30,17,17,17,17},
    {0,0,14,17,17,17,14},
    {0,0,30,17,30,16,16},
    {0,0,13,19,15,1,1},
    {0,0,22,25,16,16,16},
    {0,0,15,16,14,1,30},
    {8,8,28,8,8,9,6},
    {0,0,17,17,17,19,13},
    {0,0,17,17,17,10,4},
    {0,0,17,17,21,21,10},
    {0,0,17,10,4,10,17},
    {0,0,17,17,15,1,14},
    {0,0,31,2,4,8,31},
    {6,8,8,16,8,8,6},
    {4,4,4,4,4,4,4},
    {12,2,2,1,2,2,12},
    {0,0,8,21,2,0,0},
    {0,0,0,0,0,0,0}
};

void initializeFont()
{
    std::memset(font, 0, sizeof(font));

    for(int c = 0; c < 96; ++c)
    {
        for(int y = 0; y < 7; ++y)
        {
            for(int x = 0; x < 5; ++x)
            {
                if(fontData[c][y] & (1 << (4 - x)))
                    font[c + 32][x + 1][y] = true;
            }
        }
    }

    for(int c = 'a'; c <= 'z'; ++c)
    {
        for(int x = 0; x < 8; ++x)
        for(int y = 0; y < 8; ++y)
            font[c][x][y] = font[c - 'a' + 'A'][x][y];
    }
}

Shader::Shader() : program(0) {}

Shader::~Shader()
{
    destroy();
}

bool Shader::load(const std::string& vertexFile, const std::string& fragmentFile)
{
    destroy();

    std::string vertexSource;
    std::string fragmentSource;

    if(!loadFile(vertexSource, vertexFile) || !loadFile(fragmentSource, fragmentFile))
        return false;

    return loadSource(vertexSource, fragmentSource);
}

bool Shader::loadSource(const std::string& vertexSource, const std::string& fragmentSource)
{
    destroy();

    program = createProgram(vertexSource, fragmentSource);
    return program != 0;
}

void Shader::destroy()
{
    if(program)
    {
        glDeleteProgram(program);
        program = 0;
    }
}

void Shader::use() const
{
    if(program)
        glUseProgram(program);
}

void Shader::setInt(const std::string& name, int value)
{
    if(program)
        glUniform1i(glGetUniformLocation(program, name.c_str()), value);
}

void Shader::setFloat(const std::string& name, float value)
{
    if(program)
        glUniform1f(glGetUniformLocation(program, name.c_str()), value);
}

void Shader::setVec2(const std::string& name, float x, float y)
{
    if(program)
        glUniform2f(glGetUniformLocation(program, name.c_str()), x, y);
}

bool Shader::valid() const
{
    return program != 0;
}

uint32 Shader::id() const
{
    return program;
}

bool keyDown(int key)
{
    return window && glfwGetKey(window, key) == GLFW_PRESS;
}

bool keyPressed(int key)
{
    if(!window)
        return false;

    bool down = keyDown(key);
    bool previous = keyStates[key];
    keyStates[key] = down;
    return down && !previous;
}

bool keyReleased(int key)
{
    if(!window)
        return false;

    bool down = keyDown(key);
    bool previous = keyStates[key];
    keyStates[key] = down;
    return !down && previous;
}

void windowSizeCallback(GLFWwindow*, int newWidth, int newHeight)
{
    windowWidth = newWidth;
    windowHeight = newHeight;

    glViewport(0, 0, newWidth, newHeight);
}

void setWindowCallback(GLFWwindow* target)
{
    if(!target)
        return;

    glfwSetWindowSizeCallback(target, windowSizeCallback);
    glfwSetCharCallback(target, charCallback);
    glfwSetKeyCallback(target, keyCallback);
}

void createWindow(int renderWidth, int renderHeight, std::string& title, GLFWmonitor* monitor, bool fullscreen, int internalWidth, int internalHeight)
{
    if(window)
        return;

    width = internalWidth > 0 ? internalWidth : renderWidth;
    height = internalHeight > 0 ? internalHeight : renderHeight;

    if(!glfwInit())
    {
        std::cerr << "TinyGL: failed to initialize GLFW\n";
        std::exit(1);
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif

    GLFWmonitor* selectedMonitor = fullscreen ? (monitor ? monitor : glfwGetPrimaryMonitor()) : nullptr;

    int createWidth = renderWidth;
    int createHeight = renderHeight;

    if(selectedMonitor)
    {
        const GLFWvidmode* mode = glfwGetVideoMode(selectedMonitor);
        if(mode)
        {
            createWidth = mode->width;
            createHeight = mode->height;
        }
    }

    window = glfwCreateWindow(createWidth, createHeight, title.c_str(), selectedMonitor, nullptr);

    if(!window)
    {
        std::cerr << "TinyGL: failed to create GLFW window\n";
        glfwTerminate();
        std::exit(1);
    }

    int iconWidth = 0;
    int iconHeight = 0;
    int iconChannels = 0;
    
    unsigned char* iconPixels = stbi_load("resources/zvwolf.png", &iconWidth, &iconHeight, &iconChannels, 4);
    
    if(iconPixels)
    {
        GLFWimage icon{};
        icon.width = iconWidth;
        icon.height = iconHeight;
        icon.pixels = iconPixels;
        glfwSetWindowIcon(window, 1, &icon);
        stbi_image_free(iconPixels);
    }
    else
    {
        std::cerr << "TinyGL: failed to load window icon\n";
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(0);

    if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cerr << "TinyGL: failed to initialize GLAD\n";
        glfwDestroyWindow(window);
        window = nullptr;
        glfwTerminate();
        std::exit(1);
    }

    glfwGetWindowSize(window, &windowWidth, &windowHeight);
    setWindowCallback(window);

    glViewport(0, 0, windowWidth, windowHeight);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    initRenderer();

    if(!rendererReady)
    {
        glfwDestroyWindow(window);
        window = nullptr;
        glfwTerminate();
        std::exit(1);
    }

    initializeFont();

    isFullscreen = selectedMonitor != nullptr;
}

void toggleFullScreen(bool fullscreen)
{
    if(!window || fullscreen == isFullscreen)
        return;

    if(fullscreen)
    {
        glfwGetWindowPos(window, &windowedPosX, &windowedPosY);

        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = monitor ? glfwGetVideoMode(monitor) : nullptr;

        if(!monitor || !mode)
            return;

        glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);

        isFullscreen = true;
    }
    else
    {
        glfwSetWindowMonitor(window, nullptr, windowedPosX, windowedPosY, windowWidth > 0 ? windowWidth : width, windowHeight > 0 ? windowHeight : height, GLFW_DONT_CARE);

        isFullscreen = false;
    }
}

void render()
{
    if(!window || !rendererReady)
        return;

    glfwPollEvents();

    glClearColor(clearR, clearG, clearB, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    drawTexture();
    flushPrimitives();

    glfwSwapBuffers(window);
}

void clearScreen(const ColorRGB& color)
{
    clearR = clampColor(color.r) / 255.0f;
    clearG = clampColor(color.g) / 255.0f;
    clearB = clampColor(color.b) / 255.0f;
    hasBuffer = false;
    vertices.clear();
}

void pset(int x, int y, const ColorRGB& color)
{
    if(!onScreen(x, y))
        return;

    addQuad(float(x), float(y), float(x + 1), float(y + 1), color);
}

ColorRGB pget(int x, int y)
{
    if(!window || !rendererReady || !onScreen(x, y))
        return RGB_Black;

    flushPrimitives();

    unsigned char pixel[4] = {};
    glReadPixels(x, height - 1 - y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);

    return ColorRGB(pixel[0], pixel[1], pixel[2]);
}



bool onScreen(int x, int y)
{
    return x >= 0 && y >= 0 && x < width && y < height;
}

void sleep(double milliseconds)
{
    if(milliseconds <= 0.0)
        return;

    std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(milliseconds));
}

void waitFrame(double oldTime, double frameDuration)
{
    double elapsed = getTime() - oldTime;

    if(elapsed < frameDuration)
        sleep((frameDuration - elapsed) * 1000.0);
}

bool windowIsOpen(bool running)
{
    if(!window)
        return false;

    glfwPollEvents();

    if(glfwWindowShouldClose(window))
        return false;

    if(keyDown(GLFW_KEY_ESCAPE))
        return false;

    return running;
}

void exit()
{
    if(window)
    {
        destroyRenderer();
        glfwDestroyWindow(window);
        window = nullptr;
    }

    glfwTerminate();
    keyStates.clear();
    inputCharacters.clear();
    vertices.clear();
}

void setMouseLocked(bool locked)
{
    if(!window)
        return;

    mouseLocked = locked;

    if(locked)
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        if(glfwRawMouseMotionSupported())
            glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);

        glfwGetCursorPos(window, &lastMouseX, &lastMouseY);
    }
    else
    {
        if(glfwRawMouseMotionSupported())
            glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);

        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
}

void getMouseDelta(double& dx, double& dy)
{
    dx = 0.0;
    dy = 0.0;

    if(!window || !mouseLocked)
        return;

    double x;
    double y;

    glfwGetCursorPos(window, &x, &y);

    dx = x - lastMouseX;
    dy = y - lastMouseY;

    lastMouseX = x;
    lastMouseY = y;
}

void getMousePosition(int& mouseX, int& mouseY)
{
    mouseX = 0;
    mouseY = 0;

    if(!window)
        return;

    double x;
    double y;

    glfwGetCursorPos(window, &x, &y);

    mouseX = int(x);
    mouseY = int(y);
}

void getMousePosition(int& mouseX, int& mouseY, bool& LMB, bool& RMB)
{
    getMousePosition(mouseX, mouseY);

    LMB = window && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    RMB = window && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
}

unsigned long getTicks()
{
    return static_cast<unsigned long>(glfwGetTime() * 1000.0);
}

bool horLine(int y, int x1, int x2, const ColorRGB& color)
{
    if(y < 0 || y >= height)
        return false;

    if(x2 < x1)
        std::swap(x1, x2);

    if(x2 < 0 || x1 >= width)
        return false;

    x1 = std::max(x1, 0);
    x2 = std::min(x2, width - 1);

    addQuad(float(x1), float(y), float(x2 + 1), float(y + 1), color);

    return true;
}

bool verLine(int x, int y1, int y2, const ColorRGB& color)
{
    if(x < 0 || x >= width)
        return false;

    if(y2 < y1)
        std::swap(y1, y2);

    if(y2 < 0 || y1 >= height)
        return false;

    y1 = std::max(y1, 0);
    y2 = std::min(y2, height - 1);

    addQuad(float(x), float(y1), float(x + 1), float(y2 + 1), color);

    return true;
}

bool clipLine(int x1, int y1, int x2, int y2, int& x3, int& y3, int& x4, int& y4)
{
    if(width <= 0 || height <= 0)
        return false;

    const int left = 0;
    const int right = width - 1;
    const int top = 0;
    const int bottom = height - 1;

    auto code = [&](int x, int y)
    {
        int result = 0;

        if(x < left) result |= 1;
        if(x > right) result |= 2;
        if(y < top) result |= 4;
        if(y > bottom) result |= 8;

        return result;
    };

    double ax = x1;
    double ay = y1;
    double bx = x2;
    double by = y2;

    int a = code(x1, y1);
    int b = code(x2, y2);

    while(true)
    {
        if(!(a | b))
        {
            x3 = int(std::lround(ax));
            y3 = int(std::lround(ay));
            x4 = int(std::lround(bx));
            y4 = int(std::lround(by));
            return true;
        }

        if(a & b)
            return false;

        int outside = a ? a : b;
        double x = 0.0;
        double y = 0.0;

        if(outside & 8)
        {
            if(by == ay) return false;
            x = ax + (bx - ax) * (bottom - ay) / (by - ay);
            y = bottom;
        }
        else if(outside & 4)
        {
            if(by == ay) return false;
            x = ax + (bx - ax) * (top - ay) / (by - ay);
            y = top;
        }
        else if(outside & 2)
        {
            if(bx == ax) return false;
            y = ay + (by - ay) * (right - ax) / (bx - ax);
            x = right;
        }
        else
        {
            if(bx == ax) return false;
            y = ay + (by - ay) * (left - ax) / (bx - ax);
            x = left;
        }

        if(outside == a)
        {
            ax = x;
            ay = y;
            a = code(int(std::lround(ax)), int(std::lround(ay)));
        }
        else
        {
            bx = x;
            by = y;
            b = code(int(std::lround(bx)), int(std::lround(by)));
        }
    }
}

bool drawLine(int x1, int y1, int x2, int y2, const ColorRGB& color)
{
    int a;
    int b;
    int c;
    int d;

    if(!clipLine(x1, y1, x2, y2, a, b, c, d))
        return false;

    addLineQuad(float(a), float(b), float(c), float(d), color);
    return true;
}

bool drawCircle(int xc, int yc, int radius, const ColorRGB& color)
{
    if(radius < 0)
        return false;

    const int segments = std::max(12, std::min(96, radius * 2));

    if(radius == 0)
    {
        pset(xc, yc, color);
        return onScreen(xc, yc);
    }

    const double step = 6.28318530717958647692 / double(segments);

    for(int i = 0; i < segments; ++i)
    {
        double a1 = step * i;
        double a2 = step * (i + 1);

        float x1 = float(xc + std::cos(a1) * radius);
        float y1 = float(yc + std::sin(a1) * radius);
        float x2 = float(xc + std::cos(a2) * radius);
        float y2 = float(yc + std::sin(a2) * radius);

        addLineQuad(x1, y1, x2, y2, color);
    }

    return true;
}

bool drawDisk(int xc, int yc, int radius, const ColorRGB& color)
{
    if(radius < 0)
        return false;

    if(radius == 0)
    {
        pset(xc, yc, color);
        return onScreen(xc, yc);
    }

    const int segments = std::max(16, std::min(96, radius * 2));

    for(int i = 0; i < segments; ++i)
    {
        double a1 = 6.28318530717958647692 * i / double(segments);
        double a2 = 6.28318530717958647692 * (i + 1) / double(segments);

        addVertex(float(xc), float(yc), color);
        addVertex(float(xc + std::cos(a1) * radius), float(yc + std::sin(a1) * radius), color);
        addVertex(float(xc + std::cos(a2) * radius), float(yc + std::sin(a2) * radius), color);
    }

    return true;
}

bool drawRect(int x1, int y1, int x2, int y2, const ColorRGB& color)
{
    if(x2 < x1) std::swap(x1, x2);
    if(y2 < y1) std::swap(y1, y2);

    bool drawn = false;

    drawn |= horLine(y1, x1, x2, color);
    drawn |= horLine(y2, x1, x2, color);
    drawn |= verLine(x1, y1, y2, color);
    drawn |= verLine(x2, y1, y2, color);

    return drawn;
}

uint32 RGBtoINT(const ColorRGB& color)
{
    return packColor(color);
}

ColorRGB INTtoRGB(uint32 colorINT)
{
    return unpackColor(colorINT);
}

bool loadFile(std::vector<unsigned char>& buffer, const std::string& filename)
{
    std::ifstream file(filename, std::ios::binary | std::ios::ate);

    if(!file)
        return false;

    std::streamsize fileSize = file.tellg();

    if(fileSize < 0)
        return false;

    file.seekg(0, std::ios::beg);
    buffer.resize(size_t(fileSize));

    if(fileSize == 0)
        return true;

    return bool(file.read( reinterpret_cast<char*>(buffer.data()), fileSize ));
}

bool saveFile(const std::vector<unsigned char>& buffer, const std::string& filename)
{
    std::ofstream file(filename, std::ios::binary);

    if(!file)
        return false;

    if(buffer.empty())
        return true;

    return bool(file.write( reinterpret_cast<const char*>(buffer.data()), std::streamsize(buffer.size()) ));
}

bool loadFile(std::string& text, const std::string& filename)
{
    std::vector<unsigned char> data;

    if(!loadFile(data, filename))
        return false;

    text.assign(reinterpret_cast<const char*>(data.data()), data.size());

    return true;
}

bool saveFile(const std::string& text, const std::string& filename)
{
    std::ofstream file(filename, std::ios::binary);

    if(!file)
        return false;

    return bool(file.write(text.data(), std::streamsize(text.size())));
}





int decodePNG(std::vector<unsigned char>& out_image, unsigned long& image_width, unsigned long& image_height, const unsigned char* in_png, size in_size, bool convert_to_rgba32)
{
    if(!in_png || in_size == 0)
        return 1;

    int w = 0;
    int h = 0;
    int channels = 0;

    int desiredChannels = convert_to_rgba32 ? 4 : 0;

    stbi_uc* data = stbi_load_from_memory(in_png, int(in_size), &w, &h, &channels, desiredChannels);

    if(!data)
        return 1;

    int outputChannels = convert_to_rgba32 ? 4 : channels;

    image_width = static_cast<unsigned long>(w);
    image_height = static_cast<unsigned long>(h);

    size_t outputSize = size_t(w) * size_t(h) * size_t(outputChannels);
    out_image.assign(data, data + outputSize);

    stbi_image_free(data);
    return 0;
}



bool font[256][8][8] = {};

void drawLetter(unsigned char n, int x, int y, const ColorRGB& color, bool bg, const ColorRGB& color2)
{
    for(int v = 0; v < 8; ++v)
    {
        for(int u = 0; u < 8; ++u)
        {
            if(font[n][u][v])
                pset(x + u, y + v, color);
            else if(bg)
                pset(x + u, y + v, color2);
        }
    }
}

int printString(const std::string& text, int x, int y, const ColorRGB& color, bool bg, const ColorRGB& color2, int forceLength)
{
    int amount = 0;

    for(unsigned char c : text)
    {
        if(c == '\n')
        {
            x = 0;
            y += 8;
            continue;
        }

        ++amount;
        drawLetter(c, x, y, color, bg, color2);

        x += 8;

        if(x > width - 8)
        {
            x %= 8;
            y += 8;
        }

        if(y > height - 8)
            y %= 8;
    }

    while(amount < forceLength)
    {
        ++amount;
        drawLetter(' ', x, y, color, bg, color2);

        x += 8;

        if(x > width - 8)
        {
            x %= 8;
            y += 8;
        }

        if(y > height - 8)
            y %= 8;
    }

    return height * x + y;
}

uint8 getInputCharacter()
{
    if(inputCharacters.empty())
        return 0;

    uint8 character = inputCharacters.front();
    inputCharacters.erase(inputCharacters.begin());
    return character;
}

void getInputString(std::string& text, const std::string& message, bool clear, int x, int y, const ColorRGB& color, bool bg, const ColorRGB& color2)
{
    text.clear();

    while(window && !glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        while(!inputCharacters.empty())
        {
            uint8 c = getInputCharacter();

            if(c == 13)
                goto inputDone;

            if(c == 8)
            {
                if(!text.empty())
                    text.pop_back();

                continue;
            }

            if(c >= 32 && c <= 126)
                text.push_back(char(c));
        }

        clearScreen(RGB_Black);

        printString(message, x, y, color, bg, color2);
        printString(text, x + int(message.size()) * 8, y, color, bg, color2);

        render();
        sleep(1.0);
    }

inputDone:

    if(clear)
        clearScreen(RGB_Black);
}

// Raycaster compatibility Code for the backend of zvwolf

void drawBuffer(uint32* buffer)
{
    if(!buffer || !rendererReady)
        return;

    flushPrimitives();

    if(textureWidth != width || textureHeight != height)
        resizeTexture(width, height);

    glBindTexture(GL_TEXTURE_2D, screenTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, buffer);

    hasBuffer = true;
}

int loadImage(std::vector<uint32>& out, unsigned long& w, unsigned long& h, const std::string& filename)
{
    int imageWidth = 0;
    int imageHeight = 0;
    int channels = 0;

    stbi_uc* data = stbi_load(filename.c_str(), &imageWidth, &imageHeight, &channels, 4);

    if(!data)
        return 1;

    w = static_cast<unsigned long>(imageWidth);
    h = static_cast<unsigned long>(imageHeight);

    size_t count = size_t(imageWidth) * size_t(imageHeight);
    out.resize(count);

    for(size_t i = 0; i < count; ++i)
    {
        uint32 r = data[i * 4 + 0];
        uint32 g = data[i * 4 + 1];
        uint32 b = data[i * 4 + 2];
        uint32 a = data[i * 4 + 3];

        out[i] = (a << 24) | (b << 16) | (g << 8) | r;
    }

    stbi_image_free(data);
    return 0;
}

int loadImage(std::vector<ColorRGB>& out, unsigned long& w, unsigned long& h, const std::string& filename)
{
    std::vector<uint32> packed;

    int result = loadImage(packed, w, h, filename);

    if(result != 0)
        return result;

    out.resize(packed.size());

    for(size_t i = 0; i < packed.size(); ++i)
        out[i] = unpackColor(packed[i]);

    return 0;
}

int decodePNG(std::vector<unsigned char>& out_image_32bit, unsigned long& image_width, unsigned long& image_height, const std::vector<unsigned char>& in_png)
{
    return decodePNG(out_image_32bit, image_width, image_height, in_png.data(), in_png.size(), true);
}

// Raycaster functions

void screen(int width, int height, bool fullscreen, const std::string& text, int renderWidth, int renderHeight)
{
    std::string title = text;
    createWindow(width, height, title, nullptr, fullscreen, renderWidth, renderHeight);
}

bool done(bool quit_if_esc, bool delay)
{
    if(delay)
        sleep(5.0);

    glfwPollEvents();

    if(!window || glfwWindowShouldClose(window))
        return true;

    if(quit_if_esc && glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        return true;

    return false;
}

void redraw()
{
    render();
}

void readKeys()
{
    glfwPollEvents();
}

void toggleFullscreen(bool fullscreen, int windowedWidth, int windowedHeight)
{
    if(!window || fullscreen == isFullscreen)
        return;

    if(fullscreen)
    {
        glfwGetWindowPos(window, &windowedPosX, &windowedPosY);

        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        if(!monitor)
            return;

        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        if(!mode)
            return;

        glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);

        windowWidth = mode->width;
        windowHeight = mode->height;
        isFullscreen = true;
    }
    else
    {
        glfwSetWindowMonitor(window, nullptr, windowedPosX, windowedPosY, windowedWidth, windowedHeight, GLFW_DONT_CARE);

        windowWidth = windowedWidth;
        windowHeight = windowedHeight;
        isFullscreen = false;
    }

    glViewport(0, 0, windowWidth, windowHeight);
}
}