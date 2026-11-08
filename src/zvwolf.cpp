#include <GLFW/glfw3.h>
#include <cmath>
#include <string>
#include <vector>
#include <iostream>
#include <filesystem>

#include "tinygl.h"
using namespace TinyGL;

/*
clang++ *.cpp -O2 -opengl32 -lglfw3 -o raycasterv5.exe
g++ *.cpp -O2 lopengl32 -lglfw3 -o raycasterv5.exe
or run cmake --build build
*/

#define FLOOR_HORIZONTAL 1

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720
#define screenWidth 600
#define screenHeight 300

#define LOCK_MOUSE 1
#define MOUSE_SENSITIVITY 0.0025

#define FRAME_LIMIT 0
#define texWidth 64
#define texHeight 64
#define mapWidth 24
#define mapHeight 24

int worldMap[mapWidth][mapHeight] =
{
  {8,8,8,8,8,8,8,8,8,8,8,4,4,6,4,4,6,4,6,4,4,4,6,4},
  {8,0,0,0,0,0,0,0,0,0,8,4,0,0,0,0,0,0,0,0,0,0,0,4},
  {8,0,3,3,0,0,0,0,0,8,8,4,0,0,0,0,0,0,0,0,0,0,0,6},
  {8,0,0,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,6},
  {8,0,3,3,0,0,0,0,0,8,8,4,0,0,0,0,0,0,0,0,0,0,0,4},
  {8,0,0,0,0,0,0,0,0,0,8,4,0,0,0,0,0,6,6,6,0,6,4,6},
  {8,8,8,8,0,8,8,8,8,8,8,4,4,4,4,4,4,6,0,0,0,0,0,6},
  {7,7,7,7,0,7,7,7,7,0,8,0,8,0,8,0,8,4,0,4,0,6,0,6},
  {7,7,0,0,0,0,0,0,7,8,0,8,0,8,0,8,8,6,0,0,0,0,0,6},
  {7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,8,6,0,0,0,0,0,4},
  {7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,8,6,0,6,0,6,0,6},
  {7,7,0,0,0,0,0,0,7,8,0,8,0,8,0,8,8,6,4,6,0,6,6,6},
  {7,7,7,7,0,7,7,7,7,8,8,4,0,6,8,4,8,3,3,3,0,3,3,3},
  {2,2,2,2,0,2,2,2,2,4,6,4,0,0,6,0,6,3,0,0,0,0,0,3},
  {2,2,0,0,0,0,0,2,2,4,0,0,0,0,0,0,4,3,0,0,0,0,0,3},
  {2,0,0,0,0,0,0,0,2,4,0,0,0,0,0,0,4,3,0,0,0,0,0,3},
  {1,0,0,0,0,0,0,0,1,4,4,4,4,4,6,0,6,3,3,0,0,0,3,3},
  {2,0,0,0,0,0,0,0,2,2,2,1,2,2,2,6,6,0,0,5,0,5,0,5},
  {2,2,0,0,0,0,0,2,2,2,0,0,0,2,2,0,5,0,5,0,0,0,5,5},
  {2,0,0,0,0,0,0,0,2,0,0,0,0,0,2,5,0,5,0,5,0,5,0,5},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5},
  {2,0,0,0,0,0,0,0,2,0,0,0,0,0,2,5,0,5,0,5,0,5,0,5},
  {2,2,0,0,0,0,0,2,2,2,0,0,0,2,2,0,5,0,5,0,0,0,5,5},
  {2,2,2,2,1,2,2,2,2,2,2,1,2,2,2,5,5,5,5,5,5,5,5,5}
};

struct Sprite {
  double x, y;
  int texture;
};

#define numSprites 19

Sprite sprite[numSprites] = {
    {20.5, 11.5, 10}, //green light in front of playerstart
  //green lights in every room
  {18.5,4.5, 10},
  {10.0,4.5, 10},
  {10.0,12.5,10},
  {3.5, 6.5, 10},
  {3.5, 20.5,10},
  {3.5, 14.5,10},
  {14.5,20.5,10},

  //row of pillars in front of wall
  {18.5, 10.5, 9},
  {18.5, 11.5, 9},
  {18.5, 12.5, 9},

  //some barrels around the map
  {21.5, 1.5, 8},
  {15.5, 1.5, 8},
  {16.0, 1.8, 8},
  {16.2, 1.2, 8},
  {3.5,  2.5, 8},
  {9.5, 15.5, 8},
  {10.0, 15.1,8},
  {10.5, 15.8,8},
};

uint32 buffer[screenHeight][screenWidth];

//1D ZBuffer
float ZBuffer[screenWidth];

int spriteOrder[numSprites];
float spriteDistance[numSprites];

//function used to sort the sprites from the farthest first and the nearest last
void sortSprites(int* order, float* dist, int amount)
{
  for(int i = 1; i < amount; ++i)
  {
    const int key = order[i];
    const float keyDist = dist[key];
    int j = i - 1;
    while(j >= 0 && dist[order[j]] < keyDist)
    {
      order[j + 1] = order[j];
      --j;
    }
    order[j + 1] = key;
  }
}

int main(int, char* argv[]) {
    const std::filesystem::path executablePath = std::filesystem::absolute(argv[0]);
    const std::filesystem::path assetDir = executablePath.parent_path() / "assets";
    
  double posX = 22.0, posY = 11.5; //x and y start position
  double dirX = -1.0, dirY = 0.0; //initial direction vector
  double planeX = 0.0, planeY = 0.66; //the 2d raycaster version of camera plane

  double lastFrameTime = getTime();

  std::vector<uint32> texture[11];
  for(int i = 0; i < 11; i++) texture[i].resize(texWidth * texHeight);

  bool fullscreen = false;
  screen(WINDOW_WIDTH, WINDOW_HEIGHT, fullscreen, "Zvwolf", screenWidth, screenHeight);
#if LOCK_MOUSE
  setMouseLocked(true);
#endif

  //load some textures
  unsigned long tw, th, error = 0;
  auto loadTexture = [&](int index, const char* filename) {
      const auto path = assetDir / filename;
      const int result = loadImage(texture[index], tw, th, path.string());
      if(result != 0) { std::cerr << "Failed to load texture: " << path.string() << '\n'; ++error; }
  };
  
  loadTexture(0, "eagle.png");
  loadTexture(1, "redbrick.png");
  loadTexture(2, "purplestone.png");
  loadTexture(3, "greystone.png");
  loadTexture(4, "bluestone.png");
  loadTexture(5, "mossy.png");
  loadTexture(6, "wood.png");
  loadTexture(7, "colorstone.png");
  loadTexture(8, "barrel.png");
  loadTexture(9, "pillar.png");
  loadTexture(10, "greenlight.png");
  
  if(error != 0){ std::cerr << "Texture loading failed. " << error << " error(s).\n"; return 1; }

  while(!done(true, false))
  {
    const double frameStartTime = getTime();
    double frameTime = frameStartTime - lastFrameTime;
    lastFrameTime = frameStartTime;
    if(frameTime <= 0.0 || frameTime > 0.1) frameTime = 0.001;
#if FLOOR_HORIZONTAL
    //FLOOR CASTING
    const float rayDirX0 = static_cast<float>(dirX - planeX);
    const float rayDirY0 = static_cast<float>(dirY - planeY);
    const float rayDirX1 = static_cast<float>(dirX + planeX);
    const float rayDirY1 = static_cast<float>(dirY + planeY);
    const uint32* floorTexA = texture[3].data();
    const uint32* floorTexB = texture[4].data();
    const uint32* ceilingTex = texture[6].data();

    for(int y = screenHeight / 2 + 1; y < screenHeight; ++y)
    {
      const int p = y - screenHeight / 2;
      const float rowDistance = (0.5f * screenHeight) / p;
      const float floorStepX = rowDistance * (rayDirX1 - rayDirX0) / screenWidth;
      const float floorStepY = rowDistance * (rayDirY1 - rayDirY0) / screenWidth;
      float floorX = static_cast<float>(posX) + rowDistance * rayDirX0;
      float floorY = static_cast<float>(posY) + rowDistance * rayDirY0;

      uint32* floorRow = buffer[y];
      uint32* ceilingRow = buffer[screenHeight - y - 1];
      for(int x = 0; x < screenWidth; ++x)
      {
        const int cellX = static_cast<int>(floorX);
        const int cellY = static_cast<int>(floorY);
        const int tx = static_cast<int>(texWidth * (floorX - cellX)) & (texWidth - 1);
        const int ty = static_cast<int>(texHeight * (floorY - cellY)) & (texHeight - 1);
        floorX += floorStepX;
        floorY += floorStepY;
        const uint32* floorTex = ((cellX + cellY) & 1) ? floorTexB : floorTexA;
        floorRow[x] = (floorTex[texWidth * ty + tx] >> 1) & 8355711;
        ceilingRow[x] = (ceilingTex[texWidth * ty + tx] >> 1) & 8355711;
      }
    }
#endif // FLOOR_HORIZONTAL

    // WALL CASTING
    const float fPosX = static_cast<float>(posX);
    const float fPosY = static_cast<float>(posY);
    const float fDirX = static_cast<float>(dirX);
    const float fDirY = static_cast<float>(dirY);
    const float fPlaneX = static_cast<float>(planeX);
    const float fPlaneY = static_cast<float>(planeY);
    const float invW = 1.0f / static_cast<float>(screenWidth);
    float rayDirX = fDirX - fPlaneX;
    float rayDirY = fDirY - fPlaneY;
    const float rayStepX = (2.0f * fPlaneX) * invW;
    const float rayStepY = (2.0f * fPlaneY) * invW;
    const int startMapX = static_cast<int>(posX);
    const int startMapY = static_cast<int>(posY);
    constexpr int SHADE_MASK = 8355711;

    for(int x = 0; x < screenWidth; ++x)
    {
      int mapX = startMapX;
      int mapY = startMapY;
      float sideDistX;
      float sideDistY;
      const float deltaDistX = (rayDirX != 0.0f) ? std::fabs(1.0f / rayDirX) : 1e30f;
      const float deltaDistY = (rayDirY != 0.0f) ? std::fabs(1.0f / rayDirY) : 1e30f;
      int stepX;
      int stepY;
      int side = 0;

      if(rayDirX < 0.0f) {
        stepX = -1;
        sideDistX = (fPosX - mapX) * deltaDistX;
      } else {
        stepX = 1;
        sideDistX = (mapX + 1.0f - fPosX) * deltaDistX;
      }
      if(rayDirY < 0.0f) {
        stepY = -1;
        sideDistY = (fPosY - mapY) * deltaDistY;
      } else {
        stepY = 1;
        sideDistY = (mapY + 1.0f - fPosY) * deltaDistY;
      }

      bool hit = false;
      while(!hit)
      {
        if(sideDistX < sideDistY) {
          sideDistX += deltaDistX;
          mapX += stepX;
          side = 0;
        } else {
          sideDistY += deltaDistY;
          mapY += stepY;
          side = 1;
        }
        if(mapX < 0 || mapX >= mapWidth || mapY < 0 || mapY >= mapHeight)
          break;
        hit = worldMap[mapX][mapY] > 0;
      }

      float perpWallDist;
      int texNum;
      float wallX;
      if(!hit)
      {
        perpWallDist = 1.0f;
        texNum = 0;
        wallX = 0.0f;
      }
      else
      {
        perpWallDist = (side == 0) ? (sideDistX - deltaDistX) : (sideDistY - deltaDistY);
        texNum = worldMap[mapX][mapY] - 1;
        wallX = (side == 0) ? (fPosY + perpWallDist * rayDirY)
                            : (fPosX + perpWallDist * rayDirX);
        wallX -= std::floor(wallX);
      }

      const int lineHeight = static_cast<int>(screenHeight / perpWallDist);
      int drawStart = -lineHeight / 2 + screenHeight / 2;
      int drawEnd = lineHeight / 2 + screenHeight / 2;
      if(drawStart < 0) drawStart = 0;
      if(drawEnd >= screenHeight) drawEnd = screenHeight - 1;

      int texX = static_cast<int>(wallX * texWidth);
      if(texX < 0) texX = 0;
      if(texX >= texWidth) texX = texWidth - 1;
      if(side == 0 && rayDirX > 0.0f) texX = texWidth - texX - 1;
      if(side == 1 && rayDirY < 0.0f) texX = texWidth - texX - 1;

      const int texStep = (texHeight << 16) / ((lineHeight > 0) ? lineHeight : 1);
      int texPos = ((drawStart - screenHeight / 2 + lineHeight / 2) * texStep);
      const uint32* wallTexture = texture[texNum].data();
      const bool darkSide = (side != 0);
      for(int y = drawStart; y < drawEnd; ++y)
      {
        const int texY = (texPos >> 16) & (texHeight - 1);
        uint32 color = wallTexture[texHeight * texY + texX];
        if(darkSide) color = (color >> 1) & SHADE_MASK;
        buffer[y][x] = color;
        texPos += texStep;
      }

      ZBuffer[x] = perpWallDist;
      rayDirX += rayStepX;
      rayDirY += rayStepY;
    }

    // SPRITE CASTING
    for(int i = 0; i < numSprites; ++i)
    {
      spriteOrder[i] = i;
      const float dx = static_cast<float>(posX - sprite[i].x);
      const float dy = static_cast<float>(posY - sprite[i].y);
      spriteDistance[i] = dx * dx + dy * dy;
    }
    sortSprites(spriteOrder, spriteDistance, numSprites);

    const float invDet = 1.0f / (static_cast<float>(planeX * dirY - dirX * planeY));
    for(int i = 0; i < numSprites; ++i)
    {
      const Sprite& spr = sprite[spriteOrder[i]];
      const float spriteX = static_cast<float>(spr.x - posX);
      const float spriteY = static_cast<float>(spr.y - posY);
      const float fDirX = static_cast<float>(dirX);
      const float fDirY = static_cast<float>(dirY);
      const float fPlaneX = static_cast<float>(planeX);
      const float fPlaneY = static_cast<float>(planeY);

      const float transformX = invDet * (fDirY * spriteX - fDirX * spriteY);
      const float transformY = invDet * (-fPlaneY * spriteX + fPlaneX * spriteY);
      if(transformY <= 0.01f) continue;

      const int spriteScreenX = static_cast<int>((screenWidth * 0.5f) * (1.0f + transformX / transformY));
      const int spriteHeight = std::abs(static_cast<int>(screenHeight / transformY));
      if(spriteHeight <= 0) continue;
      const int spriteWidth = spriteHeight;

      int drawStartY = -spriteHeight / 2 + screenHeight / 2;
      int drawEndY = spriteHeight / 2 + screenHeight / 2;
      if(drawStartY < 0) drawStartY = 0;
      if(drawEndY >= screenHeight) drawEndY = screenHeight - 1;

      int drawStartX = -spriteWidth / 2 + spriteScreenX;
      int drawEndX = spriteWidth / 2 + spriteScreenX;
      if(drawStartX < 0) drawStartX = 0;
      if(drawEndX >= screenWidth) drawEndX = screenWidth - 1;
      if(drawStartX >= drawEndX) continue;

      const uint32* spriteTexture = texture[spr.texture].data();
      for(int stripe = drawStartX; stripe < drawEndX; ++stripe)
      {
        if(transformY >= ZBuffer[stripe]) continue;
        const int texX = ((stripe - (-spriteWidth / 2 + spriteScreenX)) * texWidth) / spriteWidth;
        if(texX < 0 || texX >= texWidth) continue;

        int d = (drawStartY - screenHeight / 2 + spriteHeight / 2) * 256;
        for(int y = drawStartY; y < drawEndY; ++y)
        {
          const int texY = ((d * texHeight) / spriteHeight) >> 8;
          if(texY >= 0 && texY < texHeight)
          {
            const uint32 color = spriteTexture[texWidth * texY + texX];
            if((color & 0x00FFFFFFu) != 0)
              buffer[y][stripe] = color;
          }
          d += 256;
        }
      }
    }

    drawBuffer(buffer[0]);
    static double fpsTimer = 0.0;
    static int fpsFrames = 0;
    static int displayedFPS = 0;
    fpsTimer += frameTime;
    ++fpsFrames;
    if(fpsTimer >= 0.25)
    {
      displayedFPS = static_cast<int>(fpsFrames / fpsTimer + 0.5);
      fpsTimer = 0.0;
      fpsFrames = 0;
    }
    print("FPS: ", 2, 4);
    print(displayedFPS, 32, 4);
    redraw();

    const double moveSpeed = frameTime * 3.0;
    const double rotSpeed = frameTime * 2.0;
    readKeys();

    if(keyPressed(GLFW_KEY_F11))
    {
      fullscreen = !fullscreen;
      toggleFullscreen(fullscreen, WINDOW_WIDTH, WINDOW_HEIGHT);
    }

    auto tryMove = [&](double dx, double dy)
    {
      const int testX = static_cast<int>(posX + dx);
      const int testY = static_cast<int>(posY + dy);

      if(testX >= 0 && testX < mapWidth &&
         testY >= 0 && testY < mapHeight)
      {
        if(worldMap[testX][static_cast<int>(posY)] == 0)
          posX += dx;

        if(worldMap[static_cast<int>(posX)][testY] == 0)
          posY += dy;
      }
    };

    if(keyDown(GLFW_KEY_W))
      tryMove(dirX * moveSpeed, dirY * moveSpeed);

    if(keyDown(GLFW_KEY_S))
      tryMove(-dirX * moveSpeed, -dirY * moveSpeed);

#if LOCK_MOUSE
    // A/D = strafe left/right while the mouse controls looking
    // dir is normalized, so (-dirY, dirX) is a normalized right vector
    const double rightX = -dirY;
    const double rightY = dirX;

    if(keyDown(GLFW_KEY_D))
      tryMove(-rightX * moveSpeed, -rightY * moveSpeed);

    if(keyDown(GLFW_KEY_A))
      tryMove(rightX * moveSpeed, rightY * moveSpeed);

    double mouseDX = 0.0, mouseDY = 0.0;
    getMouseDelta(mouseDX, mouseDY);
    const double mouseRotation = mouseDX * MOUSE_SENSITIVITY;

    if(mouseRotation != 0.0)
    {
      const double c = std::cos(-mouseRotation);
      const double s = std::sin(-mouseRotation);

      const double oldDirX = dirX;
      dirX = dirX * c - dirY * s;
      dirY = oldDirX * s + dirY * c;

      const double oldPlaneX = planeX;
      planeX = planeX * c - planeY * s;
      planeY = oldPlaneX * s + planeY * c;
    }
#else
    // Wolfenstein style controls A/D rotate the camera
    if(keyDown(GLFW_KEY_D))
    {
      const double c = std::cos(-rotSpeed);
      const double s = std::sin(-rotSpeed);

      const double oldDirX = dirX;
      dirX = dirX * c - dirY * s;
      dirY = oldDirX * s + dirY * c;

      const double oldPlaneX = planeX;
      planeX = planeX * c - planeY * s;
      planeY = oldPlaneX * s + planeY * c;
    }

    if(keyDown(GLFW_KEY_A))
    {
      const double c = std::cos(rotSpeed);
      const double s = std::sin(rotSpeed);

      const double oldDirX = dirX;
      dirX = dirX * c - dirY * s;
      dirY = oldDirX * s + dirY * c;

      const double oldPlaneX = planeX;
      planeX = planeX * c - planeY * s;
      planeY = oldPlaneX * s + planeY * c;
    }
#endif

#if FRAME_LIMIT > 0
    const double targetFrame = 1.0 / FRAME_LIMIT;
    waitFrame(frameStartTime, targetFrame);
#endif
  }

  return 0;
}