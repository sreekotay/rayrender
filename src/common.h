#ifndef RAYRENDER_COMMON_H
#define RAYRENDER_COMMON_H

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "light.h"

#define APP_TITLE "rayrender"
#define APP_WIDTH 1280
#define APP_HEIGHT 720
#define APP_QUALITY_MAX 2
#define APP_SCALE_MIN 0.25f
#define APP_SCALE_MAX 1.00f

typedef enum {
    SCENE_MAZE = 0,
    SCENE_VIEWER,
    SCENE_STRESS,
    SCENE_COUNT
} SceneId;

typedef struct AppState {
    SceneId scene;
    int quality;
    float scale;
    bool bilinear;
    bool wireframe;
    bool cull;
    bool showHud;
    bool adaptive;
    int binW;   /* 0 = full width (stripe) when binH > 0 */
    int binH;   /* 0 = bins off; default app uses 64 (stripe) */
    bool seq;   /* force sequential stripe fill (deny parallel) */
    int triangles;
    LightEnv light;
} AppState;

int ModelTriangleCount(Model model);
void ApplyTextureFilter(Texture2D texture, bool bilinear);
void ApplyDrawMode(const AppState *app);

Image GenMazeMap(int size);
Texture2D GenCubicmapAtlas(bool bilinear);
Image GenTerrainHeightmap(int size);
Texture2D GenTerrainAtlas(bool bilinear);
Texture2D GenCheckerTexture(int size, int cell, Color a, Color b, bool bilinear);

#endif
