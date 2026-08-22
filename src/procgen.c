#include "common.h"

#include <stdlib.h>

int ModelTriangleCount(Model model)
{
    int count = 0;
    for (int i = 0; i < model.meshCount; i++) count += model.meshes[i].triangleCount;
    return count;
}

void ApplyTextureFilter(Texture2D texture, bool bilinear)
{
    SetTextureFilter(texture, bilinear ? TEXTURE_FILTER_BILINEAR : TEXTURE_FILTER_POINT);
}

void ApplyDrawMode(const AppState *app)
{
    if (app->cull) rlEnableBackfaceCulling();
    else rlDisableBackfaceCulling();

    if (app->wireframe) rlEnableWireMode();
    else rlDisableWireMode();
}

static void DrawBrickTile(Image *image, int ox, int oy, int tw, int th, Color brick, Color mortar)
{
    ImageDrawRectangle(image, ox, oy, tw, th, mortar);

    const int rows = 8;
    const int cols = 4;
    int brickH = th / rows;
    int brickW = tw / cols;

    for (int row = 0; row < rows; row++)
    {
        int xoff = (row & 1) ? (brickW / 2) : 0;
        for (int col = -1; col <= cols; col++)
        {
            int x = ox + col * brickW + xoff + 1;
            int y = oy + row * brickH + 1;
            int w = brickW - 2;
            int h = brickH - 2;
            if (w < 1 || h < 1) continue;

            float shade = 0.82f + 0.18f * ((float)((row * 17 + col * 13) & 7) / 7.0f);
            Color fill = {
                (unsigned char)(brick.r * shade),
                (unsigned char)(brick.g * shade),
                (unsigned char)(brick.b * shade),
                255
            };
            ImageDrawRectangle(image, x, y, w, h, fill);
        }
    }
}

static void DrawNoiseTile(Image *image, int ox, int oy, int tw, int th, Color a, Color b)
{
    ImageDrawRectangle(image, ox, oy, tw, th, a);
    for (int y = 0; y < th; y++)
    {
        for (int x = 0; x < tw; x++)
        {
            int n = (x * 13 + y * 37 + (x ^ y) * 9) & 255;
            if (n > 200)
            {
                ImageDrawPixel(image, ox + x, oy + y, b);
            }
        }
    }
}

Texture2D GenCubicmapAtlas(bool bilinear)
{
    Image image = GenImageColor(256, 256, BLACK);

    DrawBrickTile(&image, 0, 0, 128, 128, (Color){ 148, 78, 52, 255 }, (Color){ 62, 40, 32, 255 });
    DrawBrickTile(&image, 128, 0, 128, 128, (Color){ 118, 64, 46, 255 }, (Color){ 48, 32, 28, 255 });
    DrawNoiseTile(&image, 0, 128, 128, 128, (Color){ 86, 82, 74, 255 }, (Color){ 58, 56, 50, 255 });
    DrawNoiseTile(&image, 128, 128, 128, 128, (Color){ 110, 96, 72, 255 }, (Color){ 78, 68, 50, 255 });

    Texture2D texture = LoadTextureFromImage(image);
    UnloadImage(image);
    ApplyTextureFilter(texture, bilinear);
    return texture;
}

Texture2D GenTerrainAtlas(bool bilinear)
{
    Image image = GenImageColor(256, 256, (Color){ 72, 110, 64, 255 });
    for (int y = 0; y < image.height; y++)
    {
        for (int x = 0; x < image.width; x++)
        {
            int n = (x * 19 + y * 11 + (x ^ y)) & 255;
            Color c = {
                (unsigned char)(58 + (n % 40)),
                (unsigned char)(92 + ((n * 3) % 50)),
                (unsigned char)(48 + (n % 28)),
                255
            };
            if (((x / 16) + (y / 16)) & 1)
            {
                c.r = (unsigned char)(c.r * 0.9f);
                c.g = (unsigned char)(c.g * 0.9f);
                c.b = (unsigned char)(c.b * 0.9f);
            }
            ImageDrawPixel(&image, x, y, c);
        }
    }

    Texture2D texture = LoadTextureFromImage(image);
    UnloadImage(image);
    ApplyTextureFilter(texture, bilinear);
    return texture;
}

Texture2D GenCheckerTexture(int size, int cell, Color a, Color b, bool bilinear)
{
    Image image = GenImageChecked(size, size, cell, cell, a, b);
    Texture2D texture = LoadTextureFromImage(image);
    UnloadImage(image);
    ApplyTextureFilter(texture, bilinear);
    return texture;
}

Image GenTerrainHeightmap(int size)
{
    return GenImagePerlinNoise(size, size, 12, 28, 3.5f);
}

Image GenMazeMap(int size)
{
    if ((size & 1) == 0) size += 1;
    if (size < 9) size = 9;

    Image image = GenImageColor(size, size, WHITE);
    Color *pixels = LoadImageColors(image);
    UnloadImage(image);

    int cellCount = (size / 2) * (size / 2);
    int *stack = (int *)MemAlloc((size_t)cellCount * sizeof(int));
    int top = 0;

    int startX = 1;
    int startY = 1;
    pixels[startY * size + startX] = BLACK;
    stack[top++] = startY * size + startX;

    const int dirs[4][2] = { { 0, -2 }, { 2, 0 }, { 0, 2 }, { -2, 0 } };

    while (top > 0)
    {
        int current = stack[top - 1];
        int cx = current % size;
        int cy = current / size;

        int order[4] = { 0, 1, 2, 3 };
        for (int i = 3; i > 0; i--)
        {
            int j = GetRandomValue(0, i);
            int tmp = order[i];
            order[i] = order[j];
            order[j] = tmp;
        }

        bool carved = false;
        for (int i = 0; i < 4; i++)
        {
            int nx = cx + dirs[order[i]][0];
            int ny = cy + dirs[order[i]][1];
            if (nx <= 0 || ny <= 0 || nx >= size - 1 || ny >= size - 1) continue;
            if (pixels[ny * size + nx].r != 255) continue;

            pixels[(cy + ny) / 2 * size + (cx + nx) / 2] = BLACK;
            pixels[ny * size + nx] = BLACK;
            stack[top++] = ny * size + nx;
            carved = true;
            break;
        }

        if (!carved) top--;
    }

    // Open a few extra rooms so first-person views have longer sightlines and more fill.
    int rooms = size / 12;
    for (int i = 0; i < rooms; i++)
    {
        int rw = GetRandomValue(3, 7);
        int rh = GetRandomValue(3, 7);
        int rx = GetRandomValue(2, size - rw - 2);
        int ry = GetRandomValue(2, size - rh - 2);
        for (int y = ry; y < ry + rh; y++)
        {
            for (int x = rx; x < rx + rw; x++)
            {
                pixels[y * size + x] = BLACK;
            }
        }
    }

    pixels[1 * size + 1] = BLACK;
    MemFree(stack);

    Image out = { 0 };
    out.data = pixels;
    out.width = size;
    out.height = size;
    out.mipmaps = 1;
    out.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    return out;
}
