#include "common.h"
#include "scenes.h"
#include "swpeek.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *SceneName(SceneId scene)
{
    switch (scene)
    {
        case SCENE_MAZE: return "maze";
        case SCENE_VIEWER: return "viewer";
        case SCENE_STRESS: return "stress";
        default: return "?";
    }
}

static const char *BackendName(void)
{
#ifdef RAYRENDER_GPU
    return "opengl 3.3";
#else
    return (rlGetVersion() == RL_OPENGL_SOFTWARE) ? "software" : "opengl";
#endif
}

static const char *ImplName(void)
{
#ifdef RAYRENDER_GPU
    return "gpu";
#elif defined(RAYRENDER_IMPL_STOCK)
    return "stock";
#else
    return "cc";
#endif
}

#if defined(RAYRENDER_IMPL_CC) && !defined(RAYRENDER_GPU)
static const char *BinLabel(const AppState *app)
{
    static char buf[32];
    if (app->binW <= 0 && app->binH <= 0) return "off";
    if (app->binW <= 0)
    {
        snprintf(buf, sizeof(buf), "hstripe×%d", app->binH);
        return buf;
    }
    if (app->binH <= 0)
    {
        snprintf(buf, sizeof(buf), "vstripe×%d", app->binW);
        return buf;
    }
    snprintf(buf, sizeof(buf), "%dx%d", app->binW, app->binH);
    return buf;
}

static void ApplyBin(AppState *app)
{
    swSetBinSize(app->binW, app->binH);
}

/* Cycle: hstripe 64 → vstripe 64 → 64×64 → 128×128 → off → … */
static void CycleBin(AppState *app)
{
    if (app->binW <= 0 && app->binH <= 0) { app->binW = 0; app->binH = 64; }
    else if (app->binW <= 0) { app->binW = 64; app->binH = 0; }
    else if (app->binH <= 0) { app->binW = 64; app->binH = 64; }
    else if (app->binW == 64) { app->binW = 128; app->binH = 128; }
    else { app->binW = 0; app->binH = 0; }
    ApplyBin(app);
}

static void ParseBinEnv(AppState *app)
{
    const char *v = getenv("RAYRENDER_BIN");
    /* Unset → keep caller default (hstripe×64). Explicit off/0 disables. */
    if (v == NULL || v[0] == '\0') return;
    if (strcmp(v, "off") == 0 || strcmp(v, "0") == 0)
    {
        app->binW = 0;
        app->binH = 0;
        return;
    }
    if (strcmp(v, "stripe") == 0 || strcmp(v, "stripe64") == 0 ||
        strcmp(v, "hstripe") == 0 || strcmp(v, "hstripe64") == 0)
    {
        app->binW = 0;
        app->binH = 64;
        return;
    }
    if (strcmp(v, "vstripe") == 0 || strcmp(v, "vstripe64") == 0)
    {
        app->binW = 64;
        app->binH = 0;
        return;
    }
    int w = 0, h = 0;
    if (sscanf(v, "%dx%d", &w, &h) == 2)
    {
        app->binW = w;
        app->binH = h;
        return;
    }
    if (sscanf(v, "%d", &h) == 1)
    {
        app->binW = h;
        app->binH = h;
    }
}
#else
static const char *BinLabel(const AppState *app)
{
    (void)app;
    return "n/a";
}
#endif

static int EnvInt(const char *name, int fallback)
{
    const char *value = getenv(name);
    if (value == NULL || value[0] == '\0') return fallback;
    return atoi(value);
}

static float EnvFloat(const char *name, float fallback)
{
    const char *value = getenv(name);
    if (value == NULL || value[0] == '\0') return fallback;
    return (float)atof(value);
}

static SceneId EnvScene(void)
{
    const char *value = getenv("RAYRENDER_SCENE");
    if (value == NULL || value[0] == '\0') return SCENE_MAZE;
    if (strcmp(value, "2") == 0 || strcmp(value, "viewer") == 0) return SCENE_VIEWER;
    if (strcmp(value, "3") == 0 || strcmp(value, "stress") == 0) return SCENE_STRESS;
    return SCENE_MAZE;
}

static uint64_t Fnv1a64(const uint8_t *data, size_t n)
{
    uint64_t hash = 1469598103934665603ULL;
    for (size_t i = 0; i < n; i++)
    {
        hash ^= data[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

#ifndef RAYRENDER_GPU
static void WritePpmRgb(const char *path, const uint8_t *rgba, int width, int height)
{
    FILE *file = fopen(path, "wb");
    if (file == NULL)
    {
        fprintf(stderr, "rayrender: cannot write %s\n", path);
        return;
    }
    fprintf(file, "P6\n%d %d\n255\n", width, height);
    for (int i = 0; i < width * height; i++) fwrite(rgba + i * 4, 1, 3, file);
    fclose(file);
}
#endif

static void SetSceneCursor(SceneId scene)
{
    if (scene == SCENE_MAZE) DisableCursor();
    else EnableCursor();
}

static void InitScenes(const AppState *app)
{
    Maze_Init(app->quality, app->bilinear);
    Viewer_Init(app->quality, app->bilinear);
    Stress_Init(app->quality, app->bilinear);
}

static void RebuildScenes(const AppState *app)
{
    Maze_Rebuild(app->quality, app->bilinear);
    Viewer_Rebuild(app->quality, app->bilinear);
    Stress_Rebuild(app->quality, app->bilinear);
}

static void ApplyFilters(const AppState *app)
{
    Maze_ApplyFilter(app->bilinear);
    Viewer_ApplyFilter(app->bilinear);
    Stress_ApplyFilter(app->bilinear);
}

static void UpdateScene(SceneId scene)
{
    switch (scene)
    {
        case SCENE_MAZE: Maze_Update(); break;
        case SCENE_VIEWER: Viewer_Update(); break;
        case SCENE_STRESS: Stress_Update(); break;
        default: break;
    }
}

static void DrawScene(AppState *app)
{
    switch (app->scene)
    {
        case SCENE_MAZE: Maze_Draw(app); break;
        case SCENE_VIEWER: Viewer_Draw(app); break;
        case SCENE_STRESS: Stress_Draw(app); break;
        default: break;
    }
}

static void DrawSceneOverlay(SceneId scene)
{
    switch (scene)
    {
        case SCENE_MAZE: Maze_DrawOverlay(); break;
        case SCENE_VIEWER: Viewer_DrawOverlay(); break;
        case SCENE_STRESS: Stress_DrawOverlay(); break;
        default: break;
    }
}

static Color SkyColor(SceneId scene)
{
    switch (scene)
    {
        case SCENE_MAZE: return (Color){ 42, 36, 48, 255 };
        case SCENE_VIEWER: return (Color){ 148, 186, 214, 255 };
        case SCENE_STRESS: return (Color){ 28, 28, 36, 255 };
        default: return BLACK;
    }
}

static void DrawHud(const AppState *app, int drawW, int drawH)
{
    if (!app->showHud) return;

    char line[192];
    int x = 16;
    int y = 16;
    int step = 20;

    DrawRectangle(8, 8, 520, 230, (Color){ 0, 0, 0, 150 });

    snprintf(line, sizeof(line), "%s  |  impl=%s  |  %s  |  %s",
             APP_TITLE, ImplName(), BackendName(), SceneName(app->scene));
    DrawText(line, x, y, 20, RAYWHITE);
    y += step + 4;

    snprintf(line, sizeof(line), "window %dx%d   draw %dx%d   scale %.2f",
             GetRenderWidth(), GetRenderHeight(), drawW, drawH, app->scale);
    DrawText(line, x, y, 16, LIGHTGRAY);
    y += step;

    snprintf(line, sizeof(line), "%.2f ms   %d fps   ~%d tris   %.2f MP",
             GetFrameTime() * 1000.0f, GetFPS(), app->triangles,
             (drawW * drawH) / 1000000.0f);
    DrawText(line, x, y, 16, LIME);
    y += step;

    snprintf(line, sizeof(line), "filter %s   wire %s   cull %s   quality %d",
             app->bilinear ? "bilinear" : "point",
             app->wireframe ? "on" : "off",
             app->cull ? "on" : "off",
             app->quality);
    DrawText(line, x, y, 16, LIGHTGRAY);
    y += step;

    snprintf(line, sizeof(line), "light %s   orbit %s   adaptive %s   bin %s   seq %s",
             app->light.enabled ? "on" : "off",
             app->light.orbit ? "on" : "off",
             app->adaptive ? "on" : "off",
             BinLabel(app),
             app->seq ? "on" : "off");
    DrawText(line, x, y, 16, LIGHTGRAY);
    y += step + 4;

    DrawText("[1/2/3] scene  [-/=] scale  ([/]) quality", x, y, 14, GRAY);
    y += 18;
    DrawText("[F] filter  [L] wire  [C] cull  [I] light  [O] orbit  [A] adaptive  [B] bin  [S] seq  [H] hud",
             x, y, 14, GRAY);
}

static RenderTexture EnsureTarget(RenderTexture target, int width, int height)
{
    if (target.id != 0 && target.texture.width == width && target.texture.height == height)
    {
        return target;
    }
    if (target.id != 0) UnloadRenderTexture(target);
    return LoadRenderTexture(width, height);
}

static void DrawFrame(AppState *app, RenderTexture *target, bool overlays)
{
    int winW = GetRenderWidth();
    int winH = GetRenderHeight();
    int drawW = (int)(winW * app->scale + 0.5f);
    int drawH = (int)(winH * app->scale + 0.5f);
    if (drawW < 160) drawW = 160;
    if (drawH < 90) drawH = 90;

    bool useTarget = app->scale < 0.999f;
    Color sky = SkyColor(app->scene);

    if (useTarget)
    {
        *target = EnsureTarget(*target, drawW, drawH);
        ApplyTextureFilter(target->texture, app->bilinear);

        BeginTextureMode(*target);
        ClearBackground(sky);
        DrawScene(app);
        EndTextureMode();

        BeginDrawing();
        ClearBackground(BLACK);
        DrawTexturePro(
            target->texture,
            (Rectangle){ 0.0f, 0.0f, (float)drawW, -(float)drawH },
            (Rectangle){ 0.0f, 0.0f, (float)winW, (float)winH },
            (Vector2){ 0.0f, 0.0f },
            0.0f,
            WHITE);
        if (overlays)
        {
            rlDisableDepthTest();
            DrawSceneOverlay(app->scene);
            DrawHud(app, drawW, drawH);
        }
        EndDrawing();
    }
    else
    {
        BeginDrawing();
        ClearBackground(sky);
        DrawScene(app);
        if (overlays)
        {
            rlDisableDepthTest();
            DrawSceneOverlay(app->scene);
            DrawHud(app, winW, winH);
        }
        EndDrawing();
    }
}

#ifndef RAYRENDER_GPU
static int RunParity(AppState *app)
{
    int frames = EnvInt("RAYRENDER_FRAMES", 2);
    if (frames < 1) frames = 1;

    const char *ppm = getenv("RAYRENDER_PPM");
    RenderTexture target = { 0 };

    app->showHud = false;
    app->light.orbit = false;
    EnableCursor();

    double t0 = GetTime();
    for (int i = 0; i < frames; i++) DrawFrame(app, &target, false);
    double timeMs = (GetTime() - t0) * 1000.0;

    int width = 0;
    int height = 0;
    const uint8_t *pixels = (const uint8_t *)swGetColorBuffer(&width, &height);
    uint64_t checksum = 0;
    if (pixels != NULL && width > 0 && height > 0)
    {
        checksum = Fnv1a64(pixels, (size_t)width * (size_t)height * 4u);
        if (ppm != NULL && ppm[0] != '\0') WritePpmRgb(ppm, pixels, width, height);
    }

    printf("rayrender impl=%s seq=1 width=%d height=%d scene=%s frames=%d checksum=0x%016llx time_ms=%.3f\n",
           ImplName(), width, height, SceneName(app->scene), frames,
           (unsigned long long)checksum, timeMs);
    fflush(stdout);

    if (target.id != 0) UnloadRenderTexture(target);
    return 0;
}
#endif

int main(void)
{
    bool parity = getenv("RAYRENDER_PARITY") != NULL;
    int width = EnvInt("RAYRENDER_WIDTH", APP_WIDTH);
    int height = EnvInt("RAYRENDER_HEIGHT", APP_HEIGHT);
    if (width < 160) width = 160;
    if (height < 90) height = 90;

    if (parity)
    {
        SetTraceLogLevel(LOG_ERROR);
        SetConfigFlags(FLAG_WINDOW_HIDDEN | FLAG_WINDOW_UNFOCUSED);
    }
    else SetConfigFlags(FLAG_WINDOW_RESIZABLE);

    char title[96];
    snprintf(title, sizeof(title), "rayrender [%s]", ImplName());
    InitWindow(width, height, title);
    SetTargetFPS(0);
    SetExitKey(KEY_Q);
    if (parity) SetRandomSeed(1);

    AppState app = {
        .scene = EnvScene(),
        .quality = EnvInt("RAYRENDER_QUALITY", parity ? 0 : APP_QUALITY_MAX),
        .scale = EnvFloat("RAYRENDER_SCALE", 1.0f),
        .bilinear = false,
        .wireframe = false,
        .cull = true,
        .showHud = true,
        .adaptive = true,
        .binW = 0,
        .binH = 64, /* default: full-width row stripe */
        .seq = false,
        .triangles = 0
    };
    {
        const char *filt = getenv("RAYRENDER_FILTER");
        if (filt != NULL && (filt[0] == '1' || strcmp(filt, "bilinear") == 0 || strcmp(filt, "linear") == 0))
            app.bilinear = true;
    }
    if (app.quality < 0) app.quality = 0;
    if (app.quality > APP_QUALITY_MAX) app.quality = APP_QUALITY_MAX;
    if (app.scale < APP_SCALE_MIN) app.scale = APP_SCALE_MIN;
    if (app.scale > APP_SCALE_MAX) app.scale = APP_SCALE_MAX;

    Light_Init(&app.light);
    if (parity) app.light.orbit = false;

#if defined(RAYRENDER_IMPL_CC) && !defined(RAYRENDER_GPU)
    {
        const char *adaptive = getenv("RAYRENDER_ADAPTIVE");
        if (adaptive != NULL && adaptive[0] == '0') app.adaptive = false;
        swSetAdaptiveAffine(app.adaptive);
        ParseBinEnv(&app);
        ApplyBin(&app);
        {
            const char *seq = getenv("RAYRENDER_SEQ");
            if (seq == NULL) seq = getenv("RLSW_SEQ");
            if (seq != NULL && seq[0] == '1') app.seq = true;
            swSetSeq(app.seq);
        }
    }
#endif

    InitScenes(&app);
    if (!parity) SetSceneCursor(app.scene);

#ifndef RAYRENDER_GPU
    if (parity)
    {
        int rc = RunParity(&app);
        Maze_Shutdown();
        Viewer_Shutdown();
        Stress_Shutdown();
        CloseWindow();
        return rc;
    }
#endif

    RenderTexture target = { 0 };

    while (!WindowShouldClose())
    {
        if (IsKeyPressed(KEY_ONE)) { app.scene = SCENE_MAZE; SetSceneCursor(app.scene); }
        if (IsKeyPressed(KEY_TWO)) { app.scene = SCENE_VIEWER; SetSceneCursor(app.scene); }
        if (IsKeyPressed(KEY_THREE)) { app.scene = SCENE_STRESS; SetSceneCursor(app.scene); }
        if (IsKeyPressed(KEY_TAB))
        {
            app.scene = (SceneId)((app.scene + 1) % SCENE_COUNT);
            SetSceneCursor(app.scene);
        }

        if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD))
        {
            app.scale += 0.25f;
            if (app.scale > APP_SCALE_MAX) app.scale = APP_SCALE_MAX;
        }
        if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT))
        {
            app.scale -= 0.25f;
            if (app.scale < APP_SCALE_MIN) app.scale = APP_SCALE_MIN;
        }

        if (IsKeyPressed(KEY_LEFT_BRACKET))
        {
            if (app.quality > 0)
            {
                app.quality--;
                RebuildScenes(&app);
            }
        }
        if (IsKeyPressed(KEY_RIGHT_BRACKET))
        {
            if (app.quality < APP_QUALITY_MAX)
            {
                app.quality++;
                RebuildScenes(&app);
            }
        }

        if (IsKeyPressed(KEY_F))
        {
            app.bilinear = !app.bilinear;
            ApplyFilters(&app);
        }
        if (IsKeyPressed(KEY_L)) app.wireframe = !app.wireframe;
        if (IsKeyPressed(KEY_C)) app.cull = !app.cull;
        if (IsKeyPressed(KEY_H)) app.showHud = !app.showHud;
        if (IsKeyPressed(KEY_I))
        {
            app.light.enabled = !app.light.enabled;
            app.light.revision++;
        }
        if (IsKeyPressed(KEY_O)) app.light.orbit = !app.light.orbit;
#if defined(RAYRENDER_IMPL_CC) && !defined(RAYRENDER_GPU)
        if (IsKeyPressed(KEY_A))
        {
            app.adaptive = !app.adaptive;
            swSetAdaptiveAffine(app.adaptive);
        }
        if (IsKeyPressed(KEY_B)) CycleBin(&app);
        if (IsKeyPressed(KEY_S))
        {
            app.seq = !app.seq;
            swSetSeq(app.seq);
        }
#endif

        Light_Update(&app.light);
        UpdateScene(app.scene);

        /* setName every frame forces AppKit layout and stutters presents on macOS. */
        {
            static char prevTitle[160];
            static double lastTitleTime = 0.0;
            double now = GetTime();
            snprintf(title, sizeof(title), "rayrender [%s]  |  %s  |  %s  |  %d fps",
                     ImplName(), BackendName(), SceneName(app.scene), GetFPS());
            if ((now - lastTitleTime) > 0.25 || strcmp(title, prevTitle) != 0)
            {
                SetWindowTitle(title);
                strncpy(prevTitle, title, sizeof(prevTitle) - 1);
                prevTitle[sizeof(prevTitle) - 1] = '\0';
                lastTitleTime = now;
            }
        }

        DrawFrame(&app, &target, true);
    }

    if (target.id != 0) UnloadRenderTexture(target);
    Maze_Shutdown();
    Viewer_Shutdown();
    Stress_Shutdown();
    CloseWindow();
    return 0;
}
