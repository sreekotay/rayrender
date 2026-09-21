#ifndef RAYRENDER_LIGHT_H
#define RAYRENDER_LIGHT_H

#include "raylib.h"

#include <stdint.h>

typedef struct LightEnv {
    Vector3 direction;
    Color sun;
    Color ambient;
    bool enabled;
    bool orbit;
    uint32_t revision; /* bumps when lighting inputs change */
} LightEnv;

void Light_Init(LightEnv *light);
void Light_Update(LightEnv *light);

/* Same combined matrix DrawModel / DrawModelEx build (model.transform * T*R*S). */
Matrix Light_DrawMatrix(Model model, Vector3 position, float scale);
Matrix Light_DrawMatrixEx(Model model, Vector3 position, Vector3 rotationAxis,
                          float rotationAngleDeg, Vector3 scale);

/* Bake vertex colors in world space using `transform` (must match the draw call).
 * Normals are renormalized only when the linear part is not orthonormal. */
void Light_ApplyModel(Model model, Matrix transform, const LightEnv *light,
                      Vector3 pointPos, float pointIntensity);

Color Light_Tint(Color base, Vector3 normal, Vector3 worldPos, const LightEnv *light,
                 Vector3 pointPos, float pointIntensity);
void Light_DrawSun(const LightEnv *light, Vector3 origin, float distance);

#endif
