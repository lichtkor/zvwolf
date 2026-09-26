#include "supergl.h"
#include "glad.h"
#include <GLFW/glfw3.h>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <map>
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <cstring>
#include <iterator>
#include <zlib.h>

namespace SuperGL
{

////////////////////////////////////////////////////////////////////////////////
// VARIABLES
////////////////////////////////////////////////////////////////////////////////

int w; 
int h; 

std::map<int, bool> keypressed; 
std::vector<Uint32> screenBuffer; 

GLFWwindow* window = nullptr;
int windowWidth = 0;
int windowHeight = 0;
int windowedPosX = 0;
int windowedPosY = 0;
bool mouseLocked = false;
bool isFullscreen = false;
double lastMouseX = 0.0;
double lastMouseY = 0.0;
GLuint screenTexture;
GLuint quadVAO, quadVBO, quadEBO;
GLuint shaderProgram;

// Internal helpers for the OpenGL context
const char* vertexShaderSource = R"(
    #version 330 core
    layout (location = 0) in vec2 aPos;
    layout (location = 1) in vec2 aTexCoord;
    out vec2 TexCoord;
    void main() {
        gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
        // Flip Y to match the top-left origin typical in 2D software renderers
        TexCoord = vec2(aTexCoord.x, 1.0 - aTexCoord.y); 
    }
)";

const char* fragmentShaderSource = R"(
    #version 330 core
    out vec4 FragColor;
    in vec2 TexCoord;
    uniform sampler2D screenTex;
    void main() {
        FragColor = texture(screenTex, TexCoord);
    }
)";

// Packs ColorRGB into a 32-bit integer formatted for GL_RGBA (Little Endian: ABGR)
inline Uint32 packColor(const ColorRGB& color) {
    return (255 << 24) | (color.b << 16) | (color.g << 8) | color.r;
}

// Unpacks GL_RGBA 32-bit integer back to ColorRGB
inline ColorRGB unpackColor(Uint32 colorINT) {
    return ColorRGB(colorINT & 0xFF, (colorINT >> 8) & 0xFF, (colorINT >> 16) & 0xFF);
}

////////////////////////////////////////////////////////////////////////////////
// KEYBOARD FUNCTIONS
////////////////////////////////////////////////////////////////////////////////

bool keyDown(int key) 
{
    if (!window) return false;
    return glfwGetKey(window, key) == GLFW_PRESS;
}

bool keyPressed(int key) 
{
    if (!window) return false;
    bool isDown = (glfwGetKey(window, key) == GLFW_PRESS);
    
    if (keypressed.find(key) == keypressed.end()) keypressed[key] = false;
    
    if (isDown) {
        if (!keypressed[key]) {
            keypressed[key] = true;
            return true;
        }
    } else {
        keypressed[key] = false;
    }
    return false;
}

void readKeys()
{
    glfwPollEvents();
}

void setMouseLocked(bool locked)
{
    if(!window) return;
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
    if(!window || !mouseLocked) return;

    double x, y;
    glfwGetCursorPos(window, &x, &y);
    dx = x - lastMouseX;
    dy = y - lastMouseY;
    lastMouseX = x;
    lastMouseY = y;
}

void getMouseState(int& mouseX, int& mouseY)
{
    mouseX = mouseY = 0;
    if(!window) return;
    double x, y;
    glfwGetCursorPos(window, &x, &y);
    mouseX = static_cast<int>(x);
    mouseY = static_cast<int>(y);
}

void getMouseState(int& mouseX, int& mouseY, bool& LMB, bool& RMB)
{
    getMouseState(mouseX, mouseY);
    LMB = window && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    RMB = window && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
}

////////////////////////////////////////////////////////////////////////////////
// BASIC SCREEN FUNCTIONS
////////////////////////////////////////////////////////////////////////////////

void screen(int width, int height, bool fullscreen, const std::string& text,
            int renderWidth, int renderHeight)
{
    windowWidth = width;
    windowHeight = height;
    isFullscreen = fullscreen;
    w = (renderWidth > 0) ? renderWidth : width;
    h = (renderHeight > 0) ? renderHeight : height;

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        std::exit(1);
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWmonitor* monitor = fullscreen ? glfwGetPrimaryMonitor() : nullptr;
    window = glfwCreateWindow(width, height, text.c_str(), monitor, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        std::exit(1);
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(0); // The raycaster can cap FPS itself; avoid driver/vsync stalls.

    // GLAD must be initialized BEFORE any OpenGL function is called.
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        glfwDestroyWindow(window);
        window = nullptr;
        glfwTerminate();
        std::exit(1);
    }

    glViewport(0, 0, windowWidth, windowHeight);

    screenBuffer.resize(w * h, packColor(RGB_Black));

    // Shader Compilation
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    GLint vertexOK = GL_FALSE;
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &vertexOK);
    if(vertexOK != GL_TRUE)
    {
        char log[2048] = {};
        glGetShaderInfoLog(vertexShader, sizeof(log), nullptr, log);
        std::cerr << "Vertex shader compilation failed:\n" << log << std::endl;
    }

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    GLint fragmentOK = GL_FALSE;
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &fragmentOK);
    if(fragmentOK != GL_TRUE)
    {
        char log[2048] = {};
        glGetShaderInfoLog(fragmentShader, sizeof(log), nullptr, log);
        std::cerr << "Fragment shader compilation failed:\n" << log << std::endl;
    }

    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    GLint linkOK = GL_FALSE;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &linkOK);
    if(linkOK != GL_TRUE)
    {
        char log[2048] = {};
        glGetProgramInfoLog(shaderProgram, sizeof(log), nullptr, log);
        std::cerr << "Shader program link failed:\n" << log << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // Quad setup for fullscreen texture rendering
    float vertices[] = {
        // positions   // texCoords
         1.0f,  1.0f,  1.0f, 1.0f, // top right
         1.0f, -1.0f,  1.0f, 0.0f, // bottom right
        -1.0f, -1.0f,  0.0f, 0.0f, // bottom left
        -1.0f,  1.0f,  0.0f, 1.0f  // top left 
    };
    unsigned int indices[] = {
        0, 1, 3, // first triangle
        1, 2, 3  // second triangle
    };

    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glGenBuffers(1, &quadEBO);

    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quadEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Texture setup
    glGenTextures(1, &screenTexture);
    glBindTexture(GL_TEXTURE_2D, screenTexture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, screenBuffer.data());
}

void toggleFullscreen(bool fullscreen, int windowedWidth, int windowedHeight)
{
    if(!window || fullscreen == isFullscreen) return;

    if(fullscreen)
    {
        glfwGetWindowPos(window, &windowedPosX, &windowedPosY);

        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        if(!monitor) return;

        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        if(!mode) return;

        glfwSetWindowMonitor(window, monitor,
                             0, 0,
                             mode->width, mode->height,
                             mode->refreshRate);

        windowWidth = mode->width;
        windowHeight = mode->height;
        isFullscreen = true;
    }
    else
    {
        glfwSetWindowMonitor(window, nullptr,
                             windowedPosX, windowedPosY,
                             windowedWidth, windowedHeight,
                             GLFW_DONT_CARE);

        windowWidth = windowedWidth;
        windowHeight = windowedHeight;
        isFullscreen = false;
    }

    // The CPU renderer keeps its internal resolution; only the presentation
    // viewport changes when the window changes size.
    glViewport(0, 0, windowWidth, windowHeight);
}

void redraw()
{
    // Upload CPU buffer to GPU
    glBindTexture(GL_TEXTURE_2D, screenTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, screenBuffer.data());

    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(shaderProgram);
    glBindVertexArray(quadVAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    glfwSwapBuffers(window);
}

void cls(const ColorRGB& color)
{
    Uint32 packed = packColor(color);
    std::fill(screenBuffer.begin(), screenBuffer.end(), packed);
}

void pset(int x, int y, const ColorRGB& color)
{
    if(x < 0 || y < 0 || x >= w || y >= h) return;
    screenBuffer[y * w + x] = packColor(color);
}

ColorRGB pget(int x, int y)
{
    if(x < 0 || y < 0 || x >= w || y >= h) return RGB_Black;
    return unpackColor(screenBuffer[y * w + x]);
}

void drawBuffer(Uint32* buffer)
{
    if(!buffer) return;
    std::memcpy(screenBuffer.data(), buffer, screenBuffer.size() * sizeof(Uint32));
}

void getScreenBuffer(std::vector<Uint32>& buffer)
{
    buffer = screenBuffer;
}

bool onScreen(int x, int y)
{
    return (x >= 0 && y >= 0 && x < w && y < h);
}

////////////////////////////////////////////////////////////////////////////////
// NON GRAPHICAL FUNCTIONS
////////////////////////////////////////////////////////////////////////////////

void sleep()
{
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

void sleep(double seconds)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<long long>(seconds * 1000)));
}

void waitFrame(double oldTime, double frameDuration)
{
    const double target = oldTime + frameDuration;
    while(true)
    {
        double now = glfwGetTime();
        double remaining = target - now;
        if(remaining <= 0.0) break;
        if(remaining > 0.002)
            std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<long long>((remaining - 0.001) * 1000.0)));
        else
            std::this_thread::yield();
    }
}

bool done(bool quit_if_esc, bool delay)
{
    if(delay) std::this_thread::sleep_for(std::chrono::milliseconds(5));
    glfwPollEvents();
    if (glfwWindowShouldClose(window)) return true;
    if (quit_if_esc && glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) return true;
    return false;
}

void end()
{
    glfwTerminate();
    std::exit(0);
}

unsigned long getTicks()
{
    return static_cast<unsigned long>(glfwGetTime() * 1000.0);
}


////////////////////////////////////////////////////////////////////////////////
// IMAGE LOADING
////////////////////////////////////////////////////////////////////////////////

// Small PNG loader used by SuperGL::loadImage.
// Supports non-interlaced PNGs with 8-bit channels and converts RGB/RGBA,
// grayscale and grayscale+alpha images to RGBA32. It uses zlib only for the
// DEFLATE stream contained in PNG IDAT chunks.

static std::uint32_t readBE32(const unsigned char* p)
{
    return (std::uint32_t(p[0]) << 24) |
           (std::uint32_t(p[1]) << 16) |
           (std::uint32_t(p[2]) << 8)  |
           std::uint32_t(p[3]);
}

static unsigned char paethPredictor(unsigned char a, unsigned char b, unsigned char c)
{
    int p  = int(a) + int(b) - int(c);
    int pa = std::abs(p - int(a));
    int pb = std::abs(p - int(b));
    int pc = std::abs(p - int(c));

    if(pa <= pb && pa <= pc) return a;
    if(pb <= pc) return b;
    return c;
}

static bool decodePNG_RGBA32(
    std::vector<unsigned char>& rgba,
    unsigned long& width,
    unsigned long& height,
    const std::vector<unsigned char>& png)
{
    static const unsigned char signature[8] =
        {137, 80, 78, 71, 13, 10, 26, 10};

    if(png.size() < 33 ||
       !std::equal(signature, signature + 8, png.begin()))
        return false;

    std::vector<unsigned char> compressed;
    unsigned char bitDepth = 0;
    unsigned char colorType = 0;
    unsigned char interlace = 0;

    std::size_t pos = 8;
    bool gotIHDR = false;
    bool gotIEND = false;

    while(pos + 12 <= png.size())
    {
        std::uint32_t chunkLength = readBE32(&png[pos]);
        pos += 4;

        if(pos + 4 > png.size()) return false;
        const unsigned char* type = &png[pos];
        pos += 4;

        if(pos + chunkLength + 4 > png.size()) return false;

        const unsigned char* data = &png[pos];

        if(std::memcmp(type, "IHDR", 4) == 0)
        {
            if(chunkLength != 13) return false;

            width = readBE32(data);
            height = readBE32(data + 4);

            unsigned char bitDepthLocal = data[8];
            colorType = data[9];
            unsigned char compression = data[10];
            unsigned char filterMethod = data[11];
            interlace = data[12];

            bitDepth = bitDepthLocal;

            // This loader intentionally handles the common 8-bit,
            // non-interlaced PNG formats.
            if(bitDepth != 8 ||
               compression != 0 ||
               filterMethod != 0 ||
               interlace != 0)
                return false;

            // Supported PNG color types:
            // 0 = grayscale
            // 2 = RGB
            // 4 = grayscale + alpha
            // 6 = RGBA
            if(colorType != 0 && colorType != 2 &&
               colorType != 4 && colorType != 6)
                return false;

            gotIHDR = true;
        }
        else if(std::memcmp(type, "IDAT", 4) == 0)
        {
            compressed.insert(compressed.end(), data, data + chunkLength);
        }
        else if(std::memcmp(type, "IEND", 4) == 0)
        {
            gotIEND = true;
            break;
        }

        // Skip chunk data and CRC.
        pos += chunkLength + 4;
    }

    if(!gotIHDR || !gotIEND || width == 0 || height == 0 ||
       compressed.empty())
        return false;

    int channels = 0;
    switch(colorType)
    {
        case 0: channels = 1; break;
        case 2: channels = 3; break;
        case 4: channels = 2; break;
        case 6: channels = 4; break;
        default: return false;
    }

    const std::size_t rowBytes = std::size_t(width) * channels;
    const std::size_t filteredSize =
        std::size_t(height) * (rowBytes + 1);

    std::vector<unsigned char> filtered(filteredSize);

    uLongf decompressedSize = static_cast<uLongf>(filtered.size());
    int zresult = uncompress(
        filtered.data(),
        &decompressedSize,
        compressed.data(),
        static_cast<uLong>(compressed.size())
    );

    if(zresult != Z_OK || decompressedSize != filtered.size())
        return false;

    // Undo PNG's per-row filters.
    std::vector<unsigned char> raw(std::size_t(height) * rowBytes);

    for(unsigned long y = 0; y < height; ++y)
    {
        const unsigned char filter = filtered[y * (rowBytes + 1)];
        const unsigned char* in =
            &filtered[y * (rowBytes + 1) + 1];
        unsigned char* out =
            &raw[y * rowBytes];

        const unsigned char* previous =
            (y == 0) ? nullptr : &raw[(y - 1) * rowBytes];

        for(std::size_t x = 0; x < rowBytes; ++x)
        {
            unsigned char left =
                (x >= std::size_t(channels)) ? out[x - channels] : 0;
            unsigned char up =
                previous ? previous[x] : 0;
            unsigned char upLeft =
                (previous && x >= std::size_t(channels))
                    ? previous[x - channels] : 0;

            switch(filter)
            {
                case 0: // None
                    out[x] = in[x];
                    break;

                case 1: // Sub
                    out[x] = static_cast<unsigned char>(in[x] + left);
                    break;

                case 2: // Up
                    out[x] = static_cast<unsigned char>(in[x] + up);
                    break;

                case 3: // Average
                    out[x] = static_cast<unsigned char>(
                        in[x] + ((int(left) + int(up)) / 2));
                    break;

                case 4: // Paeth
                    out[x] = static_cast<unsigned char>(
                        in[x] + paethPredictor(left, up, upLeft));
                    break;

                default:
                    return false;
            }
        }
    }

    rgba.resize(std::size_t(width) * std::size_t(height) * 4);

    for(unsigned long y = 0; y < height; ++y)
    {
        for(unsigned long x = 0; x < width; ++x)
        {
            const std::size_t src =
                (std::size_t(y) * width + x) * channels;
            const std::size_t dst =
                (std::size_t(y) * width + x) * 4;

            unsigned char r, g, b, a;

            if(colorType == 0)
            {
                r = g = b = raw[src];
                a = 255;
            }
            else if(colorType == 2)
            {
                r = raw[src + 0];
                g = raw[src + 1];
                b = raw[src + 2];
                a = 255;
            }
            else if(colorType == 4)
            {
                r = g = b = raw[src + 0];
                a = raw[src + 1];
            }
            else
            {
                r = raw[src + 0];
                g = raw[src + 1];
                b = raw[src + 2];
                a = raw[src + 3];
            }

            rgba[dst + 0] = r;
            rgba[dst + 1] = g;
            rgba[dst + 2] = b;
            rgba[dst + 3] = a;
        }
    }

    return true;
}

int loadImage(
    std::vector<std::uint32_t>& out,
    unsigned long& width,
    unsigned long& height,
    const std::string& filename)
{
    std::ifstream file(filename, std::ios::binary);

    if(!file)
    {
        std::cerr << "SuperGL: failed to open image: "
                  << filename << std::endl;
        return 1;
    }

    std::vector<unsigned char> png(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );

    std::vector<unsigned char> rgba;

    if(!decodePNG_RGBA32(rgba, width, height, png))
    {
        std::cerr << "SuperGL: failed to decode PNG: "
                  << filename << std::endl;
        return 1;
    }

    out.resize(std::size_t(width) * std::size_t(height));

    // Your framebuffer format is 0xAABBGGRR. On little-endian machines
    // this is laid out in memory as R,G,B,A, which matches GL_RGBA.
    for(std::size_t i = 0; i < out.size(); ++i)
    {
        const unsigned char r = rgba[i * 4 + 0];
        const unsigned char g = rgba[i * 4 + 1];
        const unsigned char b = rgba[i * 4 + 2];
        const unsigned char a = rgba[i * 4 + 3];

        out[i] =
            (std::uint32_t(a) << 24) |
            (std::uint32_t(b) << 16) |
            (std::uint32_t(g) << 8)  |
             std::uint32_t(r);
    }

    return 0;
}

int loadImage(
    std::vector<ColorRGB>& out,
    unsigned long& width,
    unsigned long& height,
    const std::string& filename)
{
    std::vector<std::uint32_t> rgba;
    int result = loadImage(rgba, width, height, filename);

    if(result != 0)
        return result;

    out.resize(rgba.size());

    for(std::size_t i = 0; i < rgba.size(); ++i)
        out[i] = unpackColor(rgba[i]);

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
// 8x8 BITMAP FONT
////////////////////////////////////////////////////////////////////////////////

// The original QuickCG API exposes an 8x8 font. For now SuperGL uses a compact
// 5x7 ASCII font and leaves one pixel of spacing around each glyph.
//
// Each character is represented by 7 rows. The low 5 bits are the pixels.
// This covers the characters needed by the raycaster/FPS display and the
// normal printable ASCII set used by simple SuperGL applications.

static const unsigned char font5x7[95][7] =
{
    {0,0,0,0,0,0,0},                 // space
    {4,4,4,4,4,0,4},                 // !
    {10,10,0,0,0,0,0},               // "
    {10,31,10,10,31,10,0},           // #
    {4,15,20,14,5,30,4},             // $
    {24,25,2,4,19,3,0},              // %
    {12,18,20,8,21,18,13},           // &
    {4,4,0,0,0,0,0},                 // '
    {2,4,8,8,8,4,2},                 // (
    {8,4,2,2,2,4,8},                 // )
    {0,4,21,14,21,4,0},              // *
    {0,4,4,31,4,4,0},                // +
    {0,0,0,0,4,4,8},                 // ,
    {0,0,0,31,0,0,0},                // -
    {0,0,0,0,0,6,6},                 // .
    {1,2,2,4,8,8,16},                // /
    {14,17,19,21,25,17,14},          // 0
    {4,12,4,4,4,4,14},               // 1
    {14,17,1,2,4,8,31},              // 2
    {30,1,1,14,1,1,30},               // 3
    {2,6,10,18,31,2,2},              // 4
    {31,16,16,30,1,1,30},             // 5
    {14,16,16,30,17,17,14},           // 6
    {31,1,2,4,8,8,8},                // 7
    {14,17,17,14,17,17,14},           // 8
    {14,17,17,15,1,1,14},             // 9
    {0,6,6,0,6,6,0},                 // :
    {0,6,6,0,6,6,8},                 // ;
    {2,4,8,16,8,4,2},                // <
    {0,0,31,0,31,0,0},               // =
    {8,4,2,1,2,4,8},                 // >
    {14,17,1,2,4,0,4},               // ?
    {14,17,1,13,21,21,14},            // @
    {14,17,17,31,17,17,17},           // A
    {30,17,17,30,17,17,30},           // B
    {14,17,16,16,16,17,14},           // C
    {30,17,17,17,17,17,30},           // D
    {31,16,16,30,16,16,31},           // E
    {31,16,16,30,16,16,16},           // F
    {14,17,16,23,17,17,15},           // G
    {17,17,17,31,17,17,17},           // H
    {14,4,4,4,4,4,14},               // I
    {7,2,2,2,2,18,12},               // J
    {17,18,20,24,20,18,17},           // K
    {16,16,16,16,16,16,31},           // L
    {17,27,21,17,17,17,17},           // M
    {17,25,21,19,17,17,17},           // N
    {14,17,17,17,17,17,14},           // O
    {30,17,17,30,16,16,16},           // P
    {14,17,17,17,21,18,13},           // Q
    {30,17,17,30,20,18,17},           // R
    {15,16,16,14,1,1,30},              // S
    {31,4,4,4,4,4,4},                // T
    {17,17,17,17,17,17,14},           // U
    {17,17,17,17,17,10,4},            // V
    {17,17,17,17,21,21,10},           // W
    {17,17,10,4,10,17,17},            // X
    {17,17,10,4,4,4,4},               // Y
    {31,1,2,4,8,16,31},               // Z
    {14,8,8,8,8,8,14},               // [
    {16,8,8,4,2,2,1},                // backslash
    {14,2,2,2,2,2,14},               // ]
    {4,10,17,0,0,0,0},               // ^
    {0,0,0,0,0,0,31},                // _
    {8,4,0,0,0,0,0},                 // `
    {0,0,14,1,15,17,15},              // a
    {16,16,22,25,17,17,30},           // b
    {0,0,14,16,16,17,14},             // c
    {1,1,13,19,17,17,15},             // d
    {0,0,14,17,31,16,14},             // e
    {6,9,8,28,8,8,8},                 // f
    {0,0,15,17,15,1,14},              // g
    {16,16,22,25,17,17,17},           // h
    {4,0,12,4,4,4,14},                // i
    {2,0,6,2,2,18,12},                // j
    {16,16,18,20,24,20,18},           // k
    {12,4,4,4,4,4,14},                // l
    {0,0,26,21,21,17,17},             // m
    {0,0,22,25,17,17,17},             // n
    {0,0,14,17,17,17,14},             // o
    {0,0,30,17,30,16,16},             // p
    {0,0,13,19,15,1,1},               // q
    {0,0,22,25,16,16,16},             // r
    {0,0,15,16,14,1,30},              // s
    {8,8,28,8,8,9,6},                 // t
    {0,0,17,17,17,19,13},             // u
    {0,0,17,17,17,10,4},              // v
    {0,0,17,17,21,21,10},             // w
    {0,0,17,10,4,10,17},              // x
    {0,0,17,17,15,1,14},              // y
    {0,0,31,2,4,8,31},                // z
    {2,4,4,8,4,4,2},                  // {
    {4,4,4,4,4,4,4},                 // |
    {8,4,4,2,4,4,8},                  // }
    {8,21,2,0,0,0,0}                 // ~
};

bool font[256][8][8] = {};

void drawLetter(
    unsigned char n,
    int x,
    int y,
    const ColorRGB& color,
    bool bg,
    const ColorRGB& color2)
{
    // Use the compact font for printable ASCII.
    if(n >= 32 && n <= 126)
    {
        const unsigned char* glyph = font5x7[n - 32];

        for(int row = 0; row < 7; ++row)
        {
            for(int col = 0; col < 5; ++col)
            {
                bool pixel = (glyph[row] & (1 << (4 - col))) != 0;

                if(pixel)
                    pset(x + col, y + row, color);
                else if(bg)
                    pset(x + col, y + row, color2);
            }
        }
    }
    else if(bg)
    {
        for(int row = 0; row < 7; ++row)
            for(int col = 0; col < 5; ++col)
                pset(x + col, y + row, color2);
    }
}

int printString(
    const std::string& text,
    int x,
    int y,
    const ColorRGB& color,
    bool bg,
    const ColorRGB& color2,
    int forceLength)
{
    const int startX = x;
    const int glyphWidth = 6;
    const int glyphHeight = 8;

    int count = 0;

    for(unsigned char c : text)
    {
        if(forceLength > 0 && count >= forceLength)
            break;

        if(c == '\n')
        {
            x = startX;
            y += glyphHeight;
            ++count;
            continue;
        }

        drawLetter(c, x, y, color, bg, color2);
        x += glyphWidth;
        ++count;
    }

    return x - startX;
}

////////////////////////////////////////////////////////////////////////////////
// 2D SHAPES
////////////////////////////////////////////////////////////////////////////////

bool horLine(int y, int x1, int x2, const ColorRGB& color)
{
    if(x2 < x1) {x1 += x2; x2 = x1 - x2; x1 -= x2;} 
    if(x2 < 0 || x1 >= w || y < 0 || y >= h) return 0; 
    if(x1 < 0) x1 = 0; 
    if(x2 >= w) x2 = w - 1; 

    Uint32 c = packColor(color);
    int offset = y * w;
    for(int x = x1; x <= x2; x++)
    {
        screenBuffer[offset + x] = c;
    }
    return 1;
}

bool verLine(int x, int y1, int y2, const ColorRGB& color)
{
    if(y2 < y1) {y1 += y2; y2 = y1 - y2; y1 -= y2;} 
    if(y2 < 0 || y1 >= h  || x < 0 || x >= w) return 0; 
    if(y1 < 0) y1 = 0; 
    if(y2 >= h) y2 = h - 1; 

    Uint32 c = packColor(color);
    for(int y = y1; y <= y2; y++)
    {
        screenBuffer[y * w + x] = c;
    }
    return 1;
}

bool drawLine(int x1, int y1, int x2, int y2, const ColorRGB& color)
{
    if(x1 < 0 || x1 > w - 1 || x2 < 0 || x2 > w - 1 || y1 < 0 || y1 > h - 1 || y2 < 0 || y2 > h - 1) return 0;

    int deltax = std::abs(x2 - x1); 
    int deltay = std::abs(y2 - y1); 
    int x = x1; 
    int y = y1; 
    int xinc1, xinc2, yinc1, yinc2, den, num, numadd, numpixels, curpixel;

    if(x2 >= x1) { xinc1 = 1; xinc2 = 1; }
    else { xinc1 = -1; xinc2 = -1; }
    
    if(y2 >= y1) { yinc1 = 1; yinc2 = 1; }
    else { yinc1 = -1; yinc2 = -1; }

    if (deltax >= deltay) 
    {
        xinc1 = 0; 
        yinc2 = 0; 
        den = deltax;
        num = deltax / 2;
        numadd = deltay;
        numpixels = deltax; 
    }
    else 
    {
        xinc2 = 0; 
        yinc1 = 0; 
        den = deltay;
        num = deltay / 2;
        numadd = deltax;
        numpixels = deltay; 
    }
    for (curpixel = 0; curpixel <= numpixels; curpixel++)
    {
        pset(x % w, y % h, color);  
        num += numadd;  
        if (num >= den) 
        {
            num -= den; 
            x += xinc1; 
            y += yinc1; 
        }
        x += xinc2; 
        y += yinc2; 
    }
    return 1;
}

bool drawCircle(int xc, int yc, int radius, const ColorRGB& color)
{
    if(xc - radius < 0 || xc + radius >= w || yc - radius < 0 || yc + radius >= h) return 0;
    int x = 0;
    int y = radius;
    int p = 3 - (radius << 1);
    int a, b, c, d, e, f, g, hh;
    while (x <= y)
    {
        a = xc + x; b = yc + y; c = xc - x; d = yc - y;
        e = xc + y; f = yc + x; g = xc - y; hh = yc - x;
        pset(a, b, color); pset(c, d, color); pset(e, f, color); pset(g, f, color);
        if(x > 0) 
        {
            pset(a, d, color); pset(c, b, color); pset(e, hh, color); pset(g, hh, color);
        }
        if(p < 0) p += (x++ << 2) + 6;
        else p += ((x++ - y--) << 2) + 10;
    }
    return 1;
}

bool drawDisk(int xc, int yc, int radius, const ColorRGB& color)
{
    if(xc + radius < 0 || xc - radius >= w || yc + radius < 0 || yc - radius >= h) return 0; 
    int x = 0;
    int y = radius;
    int p = 3 - (radius << 1);
    int a, b, c, d, e, f, g, hh;
    int pb = yc + radius + 1, pd = yc + radius + 1; 
    while (x <= y)
    {
        a = xc + x; b = yc + y; c = xc - x; d = yc - y;
        e = xc + y; f = yc + x; g = xc - y; hh = yc - x;
        if(b != pb) horLine(b, a, c, color);
        if(d != pd) horLine(d, a, c, color);
        if(f != b)  horLine(f, e, g, color);
        if(hh != d && hh != f) horLine(hh, e, g, color);
        pb = b;
        pd = d;
        if(p < 0) p += (x++ << 2) + 6;
        else p += ((x++ - y--) << 2) + 10;
    }
    return 1;
}

bool drawRect(int x1, int y1, int x2, int y2, const ColorRGB& color)
{
    if(x1 < 0 || x1 > w - 1 || x2 < 0 || x2 > w - 1 || y1 < 0 || y1 > h - 1 || y2 < 0 || y2 > h - 1) return 0;
    for(int y = y1; y <= y2; y++) {
        horLine(y, x1, x2, color);
    }
    return 1;
}

////////////////////////////////////////////////////////////////////////////////
// COLOR STRUCTS
////////////////////////////////////////////////////////////////////////////////

ColorRGB::ColorRGB(Uint8 r, Uint8 g, Uint8 b) : r(r), g(g), b(b) {}
ColorRGB::ColorRGB() : r(0), g(0), b(0) {}

ColorRGB operator+(const ColorRGB& color, const ColorRGB& color2) {
    return ColorRGB(color.r + color2.r, color.g + color2.g, color.b + color2.b);
}

ColorRGB operator-(const ColorRGB& color, const ColorRGB& color2) {
    return ColorRGB(color.r - color2.r, color.g - color2.g, color.b - color2.b);
}

ColorRGB operator*(const ColorRGB& color, int a) {
    return ColorRGB(color.r * a, color.g * a, color.b * a);
}

ColorRGB operator*(int a, const ColorRGB& color) {
    return ColorRGB(color.r * a, color.g * a, color.b * a);
}

ColorRGB operator/(const ColorRGB& color, int a) {
    if(a == 0) return color;
    return ColorRGB(color.r / a, color.g / a, color.b / a);
}

bool operator==(const ColorRGB& color, const ColorRGB& color2) {
    return(color.r == color2.r && color.g == color2.g && color.b == color2.b);
}

bool operator!=(const ColorRGB& color, const ColorRGB& color2) {
    return(!(color.r == color2.r && color.g == color2.g && color.b == color2.b));
}

} // namespace SuperGL