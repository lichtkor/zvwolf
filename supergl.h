#ifndef SUPERGL_H
#define SUPERGL_H

#include <string>
#include <sstream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <cstddef>

namespace SuperGL
{

using Uint8 = std::uint8_t;
using Uint32 = std::uint32_t;

template<typename T>
const T template_abs(const T &a)
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
  T val;
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

  ColorRGB(std::uint8_t r, std::uint8_t g, std::uint8_t b);
  ColorRGB(const ColorRGB8bit& color);
  ColorRGB();
};

ColorRGB operator+(const ColorRGB& color, const ColorRGB& color2);
ColorRGB operator-(const ColorRGB& color, const ColorRGB& color2);
ColorRGB operator*(const ColorRGB& color, int a);
ColorRGB operator*(int a, const ColorRGB& color);
ColorRGB operator/(const ColorRGB& color, int a);
bool operator==(const ColorRGB& color, const ColorRGB& color2);
bool operator!=(const ColorRGB& color, const ColorRGB& color2);

static const ColorRGB RGB_Black    (  0,   0,   0);
static const ColorRGB RGB_Red      (255,   0,   0);
static const ColorRGB RGB_Green    (  0, 255,   0);
static const ColorRGB RGB_Blue     (  0,   0, 255);
static const ColorRGB RGB_Cyan     (  0, 255, 255);
static const ColorRGB RGB_Magenta  (255,   0, 255);
static const ColorRGB RGB_Yellow   (255, 255,   0);
static const ColorRGB RGB_White    (255, 255, 255);
static const ColorRGB RGB_Gray     (128, 128, 128);
static const ColorRGB RGB_Grey     (192, 192, 192);
static const ColorRGB RGB_Maroon   (128,   0,   0);
static const ColorRGB RGB_Darkgreen(  0, 128,   0);
static const ColorRGB RGB_Navy     (  0,   0, 128);
static const ColorRGB RGB_Teal     (  0, 128, 128);
static const ColorRGB RGB_Purple   (128,   0, 128);
static const ColorRGB RGB_Olive    (128, 128,   0);

struct ColorRGB8bit
{
  std::uint8_t r;
  std::uint8_t g;
  std::uint8_t b;

  ColorRGB8bit(std::uint8_t r, std::uint8_t g, std::uint8_t b);
  ColorRGB8bit(const ColorRGB& color);
  ColorRGB8bit();
};

struct ColorHSL
{
  int h;
  int s;
  int l;

  ColorHSL(std::uint8_t h, std::uint8_t s, std::uint8_t l);
  ColorHSL();
};

struct ColorHSV
{
  int h;
  int s;
  int v;

  ColorHSV(std::uint8_t h, std::uint8_t s, std::uint8_t v);
  ColorHSV();
};

extern int w;
extern int h;

bool keyDown(int key);
bool keyPressed(int key);

void screen(int width = 640, int height = 400, bool fullscreen = false, const std::string& text = " ",
            int renderWidth = 0, int renderHeight = 0);
void toggleFullscreen(bool fullscreen, int windowedWidth, int windowedHeight);
void lock();
void unlock();
void redraw();
void cls(const ColorRGB& color = RGB_Black);
void pset(int x, int y, const ColorRGB& color);
ColorRGB pget(int x, int y);
void drawBuffer(std::uint32_t* buffer);
bool onScreen(int x, int y);

void sleep();
void sleep(double seconds);
void waitFrame(double oldTime, double frameDuration);
bool done(bool quit_if_esc = true, bool delay = true);
void end();
void readKeys();
void setMouseLocked(bool locked);
void getMouseDelta(double& dx, double& dy);
void getMouseState(int& mouseX, int& mouseY);
void getMouseState(int& mouseX, int& mouseY, bool& LMB, bool& RMB);
unsigned long getTicks();
inline double getTime() { return getTicks() / 1000.0; }

bool horLine(int y, int x1, int x2, const ColorRGB& color);
bool verLine(int x, int y1, int y2, const ColorRGB& color);
bool drawLine(int x1, int y1, int x2, int y2, const ColorRGB& color);
bool drawCircle(int xc, int yc, int radius, const ColorRGB& color);
bool drawDisk(int xc, int yc, int radius, const ColorRGB& color);
bool drawRect(int x1, int y1, int x2, int y2, const ColorRGB& color);
bool clipLine(int x1, int y1, int x2, int y2, int & x3, int & y3, int & x4, int & y4);

ColorHSL RGBtoHSL(const ColorRGB& colorRGB);
ColorRGB HSLtoRGB(const ColorHSL& colorHSL);
ColorHSV RGBtoHSV(const ColorRGB& colorRGB);
ColorRGB HSVtoRGB(const ColorHSV& colorHSV);
std::uint32_t RGBtoINT(const ColorRGB& colorRGB);
ColorRGB INTtoRGB(std::uint32_t colorINT);

void loadFile(std::vector<unsigned char>& buffer, const std::string& filename);
void saveFile(const std::vector<unsigned char>& buffer, const std::string& filename);

int loadImage(std::vector<ColorRGB>& out, unsigned long& w, unsigned long& h, const std::string& filename);
int loadImage(std::vector<std::uint32_t>& out, unsigned long& w, unsigned long& h, const std::string& filename);
int decodePNG(std::vector<unsigned char>& out_image, unsigned long& image_width, unsigned long& image_height, const unsigned char* in_png, std::size_t in_size, bool convert_to_rgba32 = true);
int decodePNG(std::vector<unsigned char>& out_image_32bit, unsigned long& image_width, unsigned long& image_height, const std::vector<unsigned char>& in_png);

extern bool font[256][8][8];
void drawLetter(unsigned char n, int x, int y, const ColorRGB& color = RGB_White, bool bg = false, const ColorRGB& color2 = RGB_Black);
int printString(const std::string& text, int x = 0, int y = 0, const ColorRGB& color = RGB_White, bool bg = false, const ColorRGB& color2 = RGB_Black, int forceLength = 0);

template<typename T>
int print(const T& val, int x = 0, int y = 0, const ColorRGB& color = RGB_White, bool bg = false, const ColorRGB& color2 = RGB_Black, int forceLength = 0)
{
  std::string text = valtostr(val);
  return printString(text, x, y, color, bg, color2, forceLength);
}

template<typename T>
int fprint(const T& val, int length, int x = 0, int y = 0, const ColorRGB& color = RGB_White, bool bg = false, const ColorRGB& color2 = RGB_Black, int forceLength = 0)
{
  std::string text = valtostr(val, length, true);
  return printString(text, x, y, color, bg, color2, forceLength);
}

std::uint8_t getInputCharacter();
void getInputString(std::string& text, const std::string& message = "", bool clear = false, int x = 0, int y = 0, const ColorRGB& color = RGB_White, bool bg = false, const ColorRGB& color2 = RGB_Black);

template<typename T>
T getInput(const std::string& message = "", bool clear = false, int x = 0, int y = 0, const ColorRGB& color = RGB_White, bool bg = false, const ColorRGB& color2 = RGB_Black)
{
  std::string text;
  getInputString(text, message, clear, x, y, color, bg, color2);
  return strtoval<T>(text);
}

int audioOpen(int samplerate, int framesize);
void audioClose();
int audioReOpen();

void audioPushSamples(const std::vector<double>& samples, std::size_t pos, std::size_t end);

std::size_t audioSamplesShortage();
std::size_t audioSamplesOverflow();
void audioSetBufferSamplesRange(std::size_t min_samples, std::size_t max_samples);

void audioPlay(const std::vector<double>& samples);

void audioSetMode(int mode);
void audioSetVolume(double volume);

}

#endif