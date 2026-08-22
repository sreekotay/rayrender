#ifndef RAYRENDER_LIGHT_H
#define RAYRENDER_LIGHT_H

#include "raylib.h"

typedef struct LightEnv {
    Vector3 direction;
    Color sun;
    Color ambient;
    bool enabled;
    bool orbit;
} LightEnv;

void Light_Init(LightEnv *light);
void Light_Update(LightEnv *light);
void Light_ApplyModel(Model model, const LightEnv *light, Vector3 pointPos, float pointIntensity);
Color Light_Tint(Color base, Vector3 normal, Vector3 worldPos, const LightEnv *light, Vector3 pointPos, float pointIntensity);
void Light_DrawSun(const LightEnv *light, Vector3 origin, float distance);

#endif
