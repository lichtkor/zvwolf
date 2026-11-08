#ifndef TINYGL_H
#define TINYGL_H

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>
#include <iomanip>
#include <string>
#include <sstream>
#include <vector>
#include <cstdint>

#if defined(_WIN32) && defined(TINYGL_SHARED)
    #ifdef TINYGL_BUILD
        #define TINYGL_API __declspec(dllexport)
    #else
        #define TINYGL_API __declspec(dllimport)
    #endif
#else
    #define TINYGL_API
#endif

namespace TinyGL
{

using uint8 = std::uint8_t;
using uint16 = std::uint16_t;
using uint32 = std::uint32_t;
using size = std::size_t;

template<typename T>
constexpr T template_abs(const T& a)
{
    return (a < 0) ? -a : a;
}

template<typename T>
std::string valtostr(const T& val)
{
    std::ostringstream sstream;
    sstream << val;
    return sstream.str();
}

template<typename T>
T strtoval(const std::string& s)
{
    std::istringstream sstream(s);
    T val{};
    sstream >> val;
    return val;
}

template<typename T>
std::string valtostr(const T& val, int length, bool fixed = true)
{
    std::ostringstream sstream;
    if(fixed) sstream << std::fixed;
    sstream << std::setprecision(length) << val;
    return sstream.str();
}

struct ColorRGB8bit;

struct ColorRGB
{
    int r;
    int g;
    int b;

    constexpr ColorRGB(uint8 r_, uint8 g_, uint8 b_) : r(r_), g(g_), b(b_) {}
    constexpr ColorRGB(const ColorRGB8bit& color);
    constexpr ColorRGB() : r(0), g(0), b(0) {}
};

struct ColorRGB8bit
{
    uint8 r;
    uint8 g;
    uint8 b;

    constexpr ColorRGB8bit(uint8 r_, uint8 g_, uint8 b_) : r(r_), g(g_), b(b_) {}
    constexpr ColorRGB8bit(const ColorRGB& color);
    constexpr ColorRGB8bit() : r(0), g(0), b(0) {}
};

inline constexpr ColorRGB RGB_Black(0, 0, 0);
inline constexpr ColorRGB RGB_Red(255, 0, 0);
inline constexpr ColorRGB RGB_Green(0, 255, 0);
inline constexpr ColorRGB RGB_Blue(0, 0, 255);
inline constexpr ColorRGB RGB_Cyan(0, 255, 255);
inline constexpr ColorRGB RGB_Magenta(255, 0, 255);
inline constexpr ColorRGB RGB_Yellow(255, 255, 0);
inline constexpr ColorRGB RGB_White(255, 255, 255);
inline constexpr ColorRGB RGB_Gray(128, 128, 128);
inline constexpr ColorRGB RGB_Grey(192, 192, 192);
inline constexpr ColorRGB RGB_Maroon(128, 0, 0);
inline constexpr ColorRGB RGB_Darkgreen(0, 128, 0);
inline constexpr ColorRGB RGB_Navy(0, 0, 128);
inline constexpr ColorRGB RGB_Teal(0, 128, 128);
inline constexpr ColorRGB RGB_Purple(128, 0, 128);
inline constexpr ColorRGB RGB_Olive(128, 128, 0);

constexpr int clampColor(int value)
{
    return value < 0 ? 0 : (value > 255 ? 255 : value);
}

constexpr ColorRGB::ColorRGB(const ColorRGB8bit& color) : r(color.r), g(color.g), b(color.b) {}
constexpr ColorRGB8bit::ColorRGB8bit(const ColorRGB& color) : r(uint8(clampColor(color.r))), g(uint8(clampColor(color.g))), b(uint8(clampColor(color.b))) {}

constexpr ColorRGB operator+(const ColorRGB& color, const ColorRGB& color2)
{
    return ColorRGB(uint8(clampColor(color.r + color2.r)), uint8(clampColor(color.g + color2.g)), uint8(clampColor(color.b + color2.b)));
}

constexpr ColorRGB operator-(const ColorRGB& color, const ColorRGB& color2)
{
    return ColorRGB(uint8(clampColor(color.r - color2.r)), uint8(clampColor(color.g - color2.g)), uint8(clampColor(color.b - color2.b)));
}

constexpr ColorRGB operator*(const ColorRGB& color, int a)
{
    return ColorRGB(uint8(clampColor(color.r * a)), uint8(clampColor(color.g * a)), uint8(clampColor(color.b * a)));
}

constexpr ColorRGB operator*(int a, const ColorRGB& color)
{
    return color * a;
}

constexpr ColorRGB operator/(const ColorRGB& color, int a)
{
    return a == 0 ? RGB_Black : ColorRGB(uint8(clampColor(color.r / a)), uint8(clampColor(color.g / a)), uint8(clampColor(color.b / a)));
}

constexpr bool operator==(const ColorRGB& color, const ColorRGB& color2)
{
    return color.r == color2.r && color.g == color2.g && color.b == color2.b;
}

constexpr bool operator!=(const ColorRGB& color, const ColorRGB& color2)
{
    return !(color == color2);
}

class Shader
{
public:
    Shader();
    ~Shader();

    bool load(const std::string& vertexFile, const std::string& fragmentFile);
    bool loadSource(const std::string& vertexSource, const std::string& fragmentSource);
    void destroy();
    void use() const;
    void setInt(const std::string& name, int value);
    void setFloat(const std::string& name, float value);
    void setVec2(const std::string& name, float x, float y);
    bool valid() const;
    uint32 id() const;

private:
    uint32 program;
};

extern TINYGL_API int width;
extern TINYGL_API int height;

bool keyDown(int key);
bool keyPressed(int key);
bool keyReleased(int key);
void createWindow(int width, int height, std::string& title, GLFWmonitor* monitor = nullptr, bool fullscreen = false, int internalWidth = 0, int internalHeight = 0);
void toggleFullScreen(bool fullscreen);
void windowSizeCallback(GLFWwindow* window, int width, int height);
void setWindowCallback(GLFWwindow* window);
void render();
void clearScreen(const ColorRGB& color = RGB_Black);
void pset(int x, int y, const ColorRGB& color);
ColorRGB pget(int x, int y);
void drawBuffer(uint32* buffer);
bool onScreen(int x, int y);

void sleep(double milliseconds);
void waitFrame(double oldTime, double frameDuration);
bool windowIsOpen(bool running);
void exit();

void setMouseLocked(bool locked);
void getMouseDelta(double& dx, double& dy);
void getMousePosition(int& mouseX, int& mouseY);
void getMousePosition(int& mouseX, int& mouseY, bool& LMB, bool& RMB);

unsigned long getTicks();
inline double getTime() { return getTicks() / 1000.0; }

bool horLine(int y, int x1, int x2, const ColorRGB& color);
bool verLine(int x, int y1, int y2, const ColorRGB& color);
bool drawLine(int x1, int y1, int x2, int y2, const ColorRGB& color);
bool drawCircle(int xc, int yc, int radius, const ColorRGB& color);
bool drawDisk(int xc, int yc, int radius, const ColorRGB& color);
bool drawRect(int x1, int y1, int x2, int y2, const ColorRGB& color);
bool clipLine(int x1, int y1, int x2, int y2, int& x3, int& y3, int& x4, int& y4);

uint32 RGBtoINT(const ColorRGB& color);
ColorRGB INTtoRGB(uint32 colorINT);

bool loadFile(std::vector<unsigned char>& buffer, const std::string& filename);
bool saveFile(const std::vector<unsigned char>& buffer, const std::string& filename);
bool loadFile(std::string& text, const std::string& filename);
bool saveFile(const std::string& text, const std::string& filename);

int loadImage(std::vector<ColorRGB>& out, unsigned long& w, unsigned long& h, const std::string& filename);
int loadImage(std::vector<uint32>& out, unsigned long& w, unsigned long& h, const std::string& filename);
int decodePNG(std::vector<unsigned char>& out_image, unsigned long& image_width, unsigned long& image_height, const unsigned char* in_png, size in_size, bool convert_to_rgba32 = true);
int decodePNG(std::vector<unsigned char>& out_image_32bit, unsigned long& image_width, unsigned long& image_height, const std::vector<unsigned char>& in_png);

extern TINYGL_API bool font[256][8][8];
void drawLetter(unsigned char n, int x, int y, const ColorRGB& color = RGB_White, bool bg = false, const ColorRGB& color2 = RGB_Black);
int printString(const std::string& text, int x = 0, int y = 0, const ColorRGB& color = RGB_White, bool bg = false, const ColorRGB& color2 = RGB_Black, int forceLength = 0);

template<typename T>
int print(const T& val, int x = 0, int y = 0, const ColorRGB& color = RGB_White, bool bg = false, const ColorRGB& color2 = RGB_Black, int forceLength = 0)
{
    return printString(valtostr(val), x, y, color, bg, color2, forceLength);
}

template<typename T>
int fprint(const T& val, int length, int x = 0, int y = 0, const ColorRGB& color = RGB_White, bool bg = false, const ColorRGB& color2 = RGB_Black, int forceLength = 0)
{
    return printString(valtostr(val, length), x, y, color, bg, color2, forceLength);
}

uint8 getInputCharacter();
void getInputString(std::string& text, const std::string& message = "", bool clear = false, int x = 0, int y = 0, const ColorRGB& color = RGB_White, bool bg = false, const ColorRGB& color2 = RGB_Black);

template<typename T>
T getInput(const std::string& message = "", bool clear = false, int x = 0, int y = 0, const ColorRGB& color = RGB_White, bool bg = false, const ColorRGB& color2 = RGB_Black)
{
    std::string text;
    getInputString(text, message, clear, x, y, color, bg, color2);
    return strtoval<T>(text);
}

// Raycaster functions

void screen(int width = 640, int height = 400, bool fullscreen = false, const std::string& text = " ", int renderWidth = 0, int renderHeight = 0);
bool done(bool quit_if_esc = true, bool delay = true);
void redraw();
void readKeys();
void toggleFullscreen(bool fullscreen, int windowedWidth, int windowedHeight);

}

#endif