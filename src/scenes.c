#include "scenes.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int QualitySize(int quality, const int *sizes)
{
    if (quality < 0) quality = 0;
    if (quality > APP_QUALITY_MAX) quality = APP_QUALITY_MAX;
    return sizes[quality];
}

static Camera MakeCamera(Vector3 position, Vector3 target, float fovy)
{
    Camera camera = { 0 };
    camera.position = position;
    camera.target = target;
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = fovy;
    camera.projection = CAMERA_PERSPECTIVE;
    return camera;
}

static void FitCameraToBounds(Camera *camera, BoundingBox bounds)
{
    Vector3 size = Vector3Subtract(bounds.max, bounds.min);
    Vector3 center = Vector3Scale(Vector3Add(bounds.min, bounds.max), 0.5f);
    float radius = Vector3Length(size) * 0.55f;
    if (radius < 2.0f) radius = 2.0f;

    camera->target = center;
    camera->position = (Vector3){
        center.x + radius,
        center.y + radius * 0.65f,
        center.z + radius
    };
}

static BoundingBox ModelBounds(Model model)
{
    BoundingBox bounds = GetMeshBoundingBox(model.meshes[0]);
    for (int i = 1; i < model.meshCount; i++)
    {
        BoundingBox next = GetMeshBoundingBox(model.meshes[i]);
        bounds.min = Vector3Min(bounds.min, next.min);
        bounds.max = Vector3Max(bounds.max, next.max);
    }
    return bounds;
}

static void DetachDiffuse(Model *model)
{
    for (int i = 0; i < model->materialCount; i++)
    {
        model->materials[i].maps[MATERIAL_MAP_DIFFUSE].texture = (Texture2D){ 0 };
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Maze
//----------------------------------------------------------------------------------------------------------------------

static const int kMazeSizes[] = { 33, 49, 65 };

typedef struct MazeScene {
    Camera camera;
    Image mapImage;
    Texture2D mapPreview;
    Texture2D atlas;
    Model model;
    Color *pixels;
    Vector3 mapPosition;
    int playerCellX;
    int playerCellY;
} MazeScene;

static MazeScene gMaze;

static void Maze_Unload(void)
{
    if (gMaze.pixels)
    {
        UnloadImageColors(gMaze.pixels);
        gMaze.pixels = NULL;
    }
    if (gMaze.mapImage.data)
    {
        UnloadImage(gMaze.mapImage);
        gMaze.mapImage = (Image){ 0 };
    }
    if (gMaze.mapPreview.id) UnloadTexture(gMaze.mapPreview);
    if (gMaze.atlas.id) UnloadTexture(gMaze.atlas);
    if (gMaze.model.meshCount)
    {
        DetachDiffuse(&gMaze.model);
        UnloadModel(gMaze.model);
    }
    memset(&gMaze, 0, sizeof(gMaze));
}

void Maze_Rebuild(int quality, bool bilinear)
{
    Maze_Unload();

    int size = QualitySize(quality, kMazeSizes);
    gMaze.mapImage = GenMazeMap(size);
    gMaze.mapPreview = LoadTextureFromImage(gMaze.mapImage);
    ApplyTextureFilter(gMaze.mapPreview, false);

    Mesh mesh = GenMeshCubicmap(gMaze.mapImage, (Vector3){ 1.0f, 1.0f, 1.0f });
    gMaze.model = LoadModelFromMesh(mesh);
    gMaze.atlas = GenCubicmapAtlas(bilinear);
    gMaze.model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = gMaze.atlas;
    gMaze.pixels = LoadImageColors(gMaze.mapImage);
    gMaze.mapPosition = (Vector3){ 0.0f, 0.0f, 0.0f };
    gMaze.camera = MakeCamera((Vector3){ 1.0f, 0.45f, 1.0f }, (Vector3){ 2.0f, 0.45f, 1.0f }, 70.0f);
}

void Maze_Init(int quality, bool bilinear)
{
    Maze_Rebuild(quality, bilinear);
}

void Maze_ApplyFilter(bool bilinear)
{
    ApplyTextureFilter(gMaze.atlas, bilinear);
}

void Maze_Update(void)
{
    Vector3 oldPos = gMaze.camera.position;
    UpdateCamera(&gMaze.camera, CAMERA_FIRST_PERSON);

    Vector2 player = { gMaze.camera.position.x, gMaze.camera.position.z };
    float radius = 0.18f;
    int mapW = gMaze.mapImage.width;
    int mapH = gMaze.mapImage.height;

    gMaze.playerCellX = (int)(player.x - gMaze.mapPosition.x + 0.5f);
    gMaze.playerCellY = (int)(player.y - gMaze.mapPosition.z + 0.5f);
    if (gMaze.playerCellX < 0) gMaze.playerCellX = 0;
    if (gMaze.playerCellY < 0) gMaze.playerCellY = 0;
    if (gMaze.playerCellX >= mapW) gMaze.playerCellX = mapW - 1;
    if (gMaze.playerCellY >= mapH) gMaze.playerCellY = mapH - 1;

    for (int y = gMaze.playerCellY - 1; y <= gMaze.playerCellY + 1; y++)
    {
        if (y < 0 || y >= mapH) continue;
        for (int x = gMaze.playerCellX - 1; x <= gMaze.playerCellX + 1; x++)
        {
            if (x < 0 || x >= mapW) continue;
            if (gMaze.pixels[y * mapW + x].r != 255) continue;
            Rectangle cell = {
                gMaze.mapPosition.x - 0.5f + (float)x,
                gMaze.mapPosition.z - 0.5f + (float)y,
                1.0f, 1.0f
            };
            if (CheckCollisionCircleRec(player, radius, cell))
            {
                gMaze.camera.position = oldPos;
            }
        }
    }
}

void Maze_Draw(AppState *app)
{
    Matrix xform = Light_DrawMatrix(gMaze.model, gMaze.mapPosition, 1.0f);
    Light_ApplyModel(gMaze.model, xform, &app->light, gMaze.camera.position,
                     app->light.enabled ? 2.1f : 0.0f);

    BeginMode3D(gMaze.camera);
    ApplyDrawMode(app);
    DrawModel(gMaze.model, gMaze.mapPosition, 1.0f, WHITE);
    rlDisableWireMode();
    EndMode3D();

    app->triangles = ModelTriangleCount(gMaze.model);
}

void Maze_DrawOverlay(void)
{
    const int scale = 3;
    int x = GetRenderWidth() - gMaze.mapPreview.width * scale - 16;
    int y = 16;
    DrawTextureEx(gMaze.mapPreview, (Vector2){ (float)x, (float)y }, 0.0f, (float)scale, WHITE);
    DrawRectangleLines(x, y, gMaze.mapPreview.width * scale, gMaze.mapPreview.height * scale, LIME);
    DrawRectangle(x + gMaze.playerCellX * scale, y + gMaze.playerCellY * scale, scale, scale, RED);
}

void Maze_Shutdown(void)
{
    Maze_Unload();
}

//----------------------------------------------------------------------------------------------------------------------
// Viewer
//----------------------------------------------------------------------------------------------------------------------

static const int kHeightSizes[] = { 80, 128, 249 }; /* q2: ~2× tris vs prior 176 */
static const int kKnotRad[] = { 48, 72, 96 };
static const int kKnotSides[] = { 16, 24, 32 };

typedef struct ViewerScene {
    Camera camera;
    Model terrain;
    Model sculpture;
    Texture2D terrainTex;
    Texture2D sculptureTex;
    bool customModel;
    char customPath[512];
    BoundingBox bounds;
} ViewerScene;

static ViewerScene gViewer;

static void Viewer_UnloadSculpture(void)
{
    if (gViewer.sculpture.meshCount)
    {
        DetachDiffuse(&gViewer.sculpture);
        UnloadModel(gViewer.sculpture);
    }
    gViewer.sculpture = (Model){ 0 };
}

static void Viewer_Unload(void)
{
    Viewer_UnloadSculpture();
    if (gViewer.terrain.meshCount)
    {
        DetachDiffuse(&gViewer.terrain);
        UnloadModel(gViewer.terrain);
    }
    if (gViewer.terrainTex.id) UnloadTexture(gViewer.terrainTex);
    if (gViewer.sculptureTex.id) UnloadTexture(gViewer.sculptureTex);
    memset(&gViewer, 0, sizeof(gViewer));
}

static Model BuildSculpture(int quality, Texture2D texture)
{
    int rad = QualitySize(quality, kKnotRad);
    int sides = QualitySize(quality, kKnotSides);
    Model model = LoadModelFromMesh(GenMeshKnot(1.0f, 0.38f, rad, sides));
    model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = texture;
    return model;
}

void Viewer_Rebuild(int quality, bool bilinear)
{
    bool keepCustom = gViewer.customModel;
    char path[512];
    strncpy(path, gViewer.customPath, sizeof(path) - 1);
    path[sizeof(path) - 1] = '\0';

    Viewer_Unload();

    int heightSize = QualitySize(quality, kHeightSizes);
    Image height = GenTerrainHeightmap(heightSize);
    Mesh terrainMesh = GenMeshHeightmap(height, (Vector3){ 64.0f, 10.0f, 64.0f });
    UnloadImage(height);

    gViewer.terrain = LoadModelFromMesh(terrainMesh);
    gViewer.terrainTex = GenTerrainAtlas(bilinear);
    gViewer.terrain.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = gViewer.terrainTex;

    gViewer.sculptureTex = GenCheckerTexture(256, 16,
        (Color){ 210, 168, 96, 255 },
        (Color){ 86, 64, 42, 255 },
        bilinear);

    if (keepCustom && path[0] != '\0')
    {
        gViewer.sculpture = LoadModel(path);
        gViewer.sculpture.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = gViewer.sculptureTex;
        gViewer.customModel = true;
        strncpy(gViewer.customPath, path, sizeof(gViewer.customPath) - 1);
    }
    else
    {
        gViewer.sculpture = BuildSculpture(quality, gViewer.sculptureTex);
    }

    gViewer.bounds = ModelBounds(gViewer.sculpture);
    gViewer.camera = MakeCamera((Vector3){ 28.0f, 18.0f, 28.0f }, (Vector3){ 0.0f, 6.0f, 0.0f }, 55.0f);
    if (gViewer.customModel) FitCameraToBounds(&gViewer.camera, gViewer.bounds);
}

void Viewer_Init(int quality, bool bilinear)
{
    Viewer_Rebuild(quality, bilinear);
}

void Viewer_ApplyFilter(bool bilinear)
{
    ApplyTextureFilter(gViewer.terrainTex, bilinear);
    ApplyTextureFilter(gViewer.sculptureTex, bilinear);
}

void Viewer_Update(void)
{
    UpdateCamera(&gViewer.camera, CAMERA_ORBITAL);

    if (!IsFileDropped()) return;

    FilePathList dropped = LoadDroppedFiles();
    if (dropped.count == 1)
    {
        const char *path = dropped.paths[0];
        if (IsFileExtension(path, ".obj") || IsFileExtension(path, ".gltf") ||
            IsFileExtension(path, ".glb") || IsFileExtension(path, ".vox") ||
            IsFileExtension(path, ".iqm") || IsFileExtension(path, ".m3d"))
        {
            Viewer_UnloadSculpture();
            gViewer.sculpture = LoadModel(path);
            gViewer.sculpture.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = gViewer.sculptureTex;
            gViewer.customModel = true;
            strncpy(gViewer.customPath, path, sizeof(gViewer.customPath) - 1);
            gViewer.bounds = ModelBounds(gViewer.sculpture);
            FitCameraToBounds(&gViewer.camera, gViewer.bounds);
        }
        else if (IsFileExtension(path, ".png") || IsFileExtension(path, ".jpg") ||
                 IsFileExtension(path, ".jpeg") || IsFileExtension(path, ".bmp"))
        {
            UnloadTexture(gViewer.sculptureTex);
            gViewer.sculptureTex = LoadTexture(path);
            ApplyTextureFilter(gViewer.sculptureTex, true);
            gViewer.sculpture.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = gViewer.sculptureTex;
        }
    }
    UnloadDroppedFiles(dropped);
}

void Viewer_Draw(AppState *app)
{
    float sculpScale = gViewer.customModel ? 1.0f : 3.2f;
    Light_ApplyModel(gViewer.terrain,
                     Light_DrawMatrix(gViewer.terrain, (Vector3){ -32.0f, 0.0f, -32.0f }, 1.0f),
                     &app->light, (Vector3){ 0 }, 0.0f);
    Light_ApplyModel(gViewer.sculpture,
                     Light_DrawMatrix(gViewer.sculpture, (Vector3){ 0.0f, 8.5f, 0.0f }, sculpScale),
                     &app->light, (Vector3){ 0 }, 0.0f);

    BeginMode3D(gViewer.camera);
    ApplyDrawMode(app);
    DrawModel(gViewer.terrain, (Vector3){ -32.0f, 0.0f, -32.0f }, 1.0f, WHITE);
    DrawModel(gViewer.sculpture, (Vector3){ 0.0f, 8.5f, 0.0f }, sculpScale, WHITE);
    rlDisableWireMode();
    Light_DrawSun(&app->light, (Vector3){ 0.0f, 8.0f, 0.0f }, 28.0f);
    DrawGrid(20, 4.0f);
    EndMode3D();

    app->triangles = ModelTriangleCount(gViewer.terrain) + ModelTriangleCount(gViewer.sculpture);
}

void Viewer_DrawOverlay(void)
{
    DrawText("Drag & drop .obj/.gltf/.glb/.vox to replace the sculpture", 16, GetRenderHeight() - 28, 16, LIGHTGRAY);
}

void Viewer_Shutdown(void)
{
    Viewer_Unload();
}

//----------------------------------------------------------------------------------------------------------------------
// Stress
//----------------------------------------------------------------------------------------------------------------------

/* ~2× prior cube counts (cbrt(2) on the grid edge). */
static const int kCubeCounts[] = { 13, 18, 20 };
/* ~2× prior knot tessellation. */
static const int kCenterSeg[] = { 64, 96, 128 };
/* Dense UV sphere — many small tris (setup / 0–1 center / reject), SGI-sphere style. */
static const int kSphereRings[] = { 48, 72, 96 };
static const int kSphereSlices[] = { 96, 144, 192 };

typedef struct StressScene {
    Camera camera;
    Model center;
    Texture2D centerTex;
    Model sphere;
    Texture2D sphereTex;
    int blocks;
    Color *cubeColors;
    int colorCount;
} StressScene;

static StressScene gStress;

static void Stress_Unload(void)
{
    if (gStress.center.meshCount)
    {
        DetachDiffuse(&gStress.center);
        UnloadModel(gStress.center);
    }
    if (gStress.centerTex.id) UnloadTexture(gStress.centerTex);
    if (gStress.sphere.meshCount)
    {
        DetachDiffuse(&gStress.sphere);
        UnloadModel(gStress.sphere);
    }
    if (gStress.sphereTex.id) UnloadTexture(gStress.sphereTex);
    if (gStress.cubeColors) MemFree(gStress.cubeColors);
    memset(&gStress, 0, sizeof(gStress));
}

void Stress_Rebuild(int quality, bool bilinear)
{
    Stress_Unload();

    gStress.blocks = QualitySize(quality, kCubeCounts);
    int seg = QualitySize(quality, kCenterSeg);
    Mesh knot = GenMeshKnot(1.0f, 0.4f, seg * 2, seg / 2);
    gStress.center = LoadModelFromMesh(knot);
    gStress.centerTex = GenCheckerTexture(128, 8,
        (Color){ 240, 220, 80, 255 },
        (Color){ 40, 40, 48, 255 },
        bilinear);
    gStress.center.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = gStress.centerTex;

    int rings = QualitySize(quality, kSphereRings);
    int slices = QualitySize(quality, kSphereSlices);
    gStress.sphere = LoadModelFromMesh(GenMeshSphere(1.0f, rings, slices));
    gStress.sphereTex = GenCheckerTexture(64, 16,
        (Color){ 180, 210, 255, 255 },
        (Color){ 30, 50, 90, 255 },
        bilinear);
    gStress.sphere.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = gStress.sphereTex;

    gStress.colorCount = gStress.blocks * gStress.blocks * gStress.blocks;
    gStress.cubeColors = (Color *)MemAlloc((size_t)gStress.colorCount * sizeof(Color));
    int i = 0;
    for (int x = 0; x < gStress.blocks; x++)
    {
        for (int y = 0; y < gStress.blocks; y++)
        {
            for (int z = 0; z < gStress.blocks; z++)
            {
                gStress.cubeColors[i++] = ColorFromHSV((float)(((x + y + z) * 18) % 360), 0.75f, 0.9f);
            }
        }
    }

    gStress.camera = MakeCamera((Vector3){ 40.0f, 22.0f, 40.0f }, (Vector3){ 0.0f, 0.0f, 0.0f }, 65.0f);
}

void Stress_Init(int quality, bool bilinear)
{
    Stress_Rebuild(quality, bilinear);
}

void Stress_ApplyFilter(bool bilinear)
{
    ApplyTextureFilter(gStress.centerTex, bilinear);
    ApplyTextureFilter(gStress.sphereTex, bilinear);
}

void Stress_Update(void)
{
    double t = GetTime() * 0.28;
    gStress.camera.position.x = (float)(cos(t) * 42.0);
    gStress.camera.position.z = (float)(sin(t) * 42.0);
    gStress.camera.position.y = 18.0f + (float)(sin(t * 0.7) * 4.0);
}

void Stress_Draw(AppState *app)
{
    /* Hitch-clamped clock so ApplyModel spikes don't yank the wave phase. */
    static double waveClock = 0.0;
    float dt = GetFrameTime();
    if (dt < 0.0f) dt = 0.0f;
    if (dt > (1.0f / 30.0f)) dt = 1.0f / 30.0f;
    waveClock += (double)dt;

    int n = gStress.blocks;
    int cubes = 0;
    const float spacing = 3.0f;

    BeginMode3D(gStress.camera);
    ApplyDrawMode(app);

    Light_ApplyModel(gStress.center,
                     Light_DrawMatrix(gStress.center, (Vector3){ 0.0f, 0.0f, 0.0f }, 6.0f),
                     &app->light, (Vector3){ 0 }, 0.0f);
    DrawModel(gStress.center, (Vector3){ 0.0f, 0.0f, 0.0f }, 6.0f, WHITE);

    /* Shared mesh drawn at three poses — bake object-space (can't store 3 orientations). */
    Light_ApplyModel(gStress.sphere, MatrixIdentity(), &app->light, (Vector3){ 0 }, 0.0f);
    float spin = (float)(waveClock * 0.35);
    DrawModelEx(gStress.sphere, (Vector3){ 14.0f, 4.0f, 0.0f }, (Vector3){ 0, 1, 0 }, spin * RAD2DEG,
                (Vector3){ 5.5f, 5.5f, 5.5f }, WHITE);
    DrawModelEx(gStress.sphere, (Vector3){ -10.0f, 8.0f, 12.0f }, (Vector3){ 0, 1, 0 }, -spin * RAD2DEG * 0.7f,
                (Vector3){ 3.2f, 3.2f, 3.2f }, WHITE);
    DrawModelEx(gStress.sphere, (Vector3){ 6.0f, -2.0f, -18.0f }, (Vector3){ 1, 0.4f, 0 }, spin * RAD2DEG * 0.5f,
                (Vector3){ 2.0f, 2.0f, 2.0f }, WHITE);

    int idx = 0;
    for (int x = 0; x < n; x++)
    {
        for (int y = 0; y < n; y++)
        {
            for (int z = 0; z < n; z++)
            {
                float blockScale = (x + y + z) / 30.0f;
                /* Same spatial wave as before; phase from hitch-clamped clock. */
                float scatter = sinf(blockScale * 20.0f + (float)(waveClock * 4.0));
                Vector3 pos = {
                    (x - n / 2.0f) * spacing + scatter,
                    (y - n / 2.0f) * spacing * 0.75f + scatter,
                    (z - n / 2.0f) * spacing + scatter
                };
                float size = 1.1f;
                Vector3 nrm = Vector3LengthSqr(pos) > 0.0001f ? Vector3Normalize(pos) : (Vector3){ 0.0f, 1.0f, 0.0f };
                Color lit = Light_Tint(gStress.cubeColors[idx], nrm, pos, &app->light, (Vector3){ 0 }, 0.0f);
                if (app->wireframe) DrawCubeWires(pos, size, size, size, lit);
                else DrawCube(pos, size, size, size, lit);
                idx++;
                cubes++;
            }
        }
    }

    rlDisableWireMode();
    Light_DrawSun(&app->light, (Vector3){ 0.0f, 2.0f, 0.0f }, 36.0f);
    DrawGrid(16, 5.0f);
    EndMode3D();

    int sphereTris = ModelTriangleCount(gStress.sphere);
    app->triangles = ModelTriangleCount(gStress.center) + sphereTris * 3 + cubes * 12;
}

void Stress_DrawOverlay(void)
{
    char buf[96];
    int sphereTris = ModelTriangleCount(gStress.sphere);
    snprintf(buf, sizeof(buf), "cubes %dx%dx%d + sphere %d tris x3",
             gStress.blocks, gStress.blocks, gStress.blocks, sphereTris);
    DrawText(buf, 16, GetRenderHeight() - 28, 16, LIGHTGRAY);
}

void Stress_Shutdown(void)
{
    Stress_Unload();
}
