#include "light.h"

#include "raymath.h"
#include "rlgl.h"

#include <math.h>
#include <stddef.h>

static float Clamp255(float value)
{
    if (value < 0.0f) return 0.0f;
    if (value > 255.0f) return 255.0f;
    return value;
}

void Light_Init(LightEnv *light)
{
    light->direction = Vector3Normalize((Vector3){ 0.45f, -0.75f, 0.48f });
    light->sun = (Color){ 255, 214, 160, 255 };
    light->ambient = (Color){ 42, 48, 68, 255 };
    light->enabled = true;
    light->orbit = true;
}

void Light_Update(LightEnv *light)
{
    if (!light->enabled || !light->orbit) return;

    float t = (float)GetTime() * 0.32f;
    light->direction = Vector3Normalize((Vector3){ cosf(t), -0.62f, sinf(t) });
}

static Vector3 Light_Eval(Vector3 normal, Vector3 worldPos, const LightEnv *light, Vector3 pointPos, float pointIntensity)
{
    if (!light->enabled) return (Vector3){ 1.0f, 1.0f, 1.0f };

    Vector3 n = Vector3Normalize(normal);
    if (Vector3LengthSqr(n) < 0.0001f) n = (Vector3){ 0.0f, 1.0f, 0.0f };

    Vector3 toSun = Vector3Normalize(Vector3Negate(light->direction));
    float wrap = Vector3DotProduct(n, toSun) * 0.5f + 0.5f;
    if (wrap < 0.0f) wrap = 0.0f;

    Vector3 out = {
        (light->ambient.r + light->sun.r * wrap) / 255.0f,
        (light->ambient.g + light->sun.g * wrap) / 255.0f,
        (light->ambient.b + light->sun.b * wrap) / 255.0f
    };

    if (pointIntensity > 0.0f)
    {
        Vector3 toPoint = Vector3Subtract(pointPos, worldPos);
        float dist = Vector3Length(toPoint);
        if (dist > 0.0001f)
        {
            toPoint = Vector3Scale(toPoint, 1.0f / dist);
            float att = pointIntensity / (1.0f + dist * 0.38f);
            float spot = Vector3DotProduct(n, toPoint);
            if (spot < 0.0f) spot = 0.0f;
            float lamp = spot * att;
            out.x += 0.95f * lamp;
            out.y += 0.82f * lamp;
            out.z += 0.55f * lamp;
        }
    }

    return out;
}

Color Light_Tint(Color base, Vector3 normal, Vector3 worldPos, const LightEnv *light, Vector3 pointPos, float pointIntensity)
{
    Vector3 e = Light_Eval(normal, worldPos, light, pointPos, pointIntensity);
    return (Color){
        (unsigned char)Clamp255((float)base.r * e.x),
        (unsigned char)Clamp255((float)base.g * e.y),
        (unsigned char)Clamp255((float)base.b * e.z),
        base.a
    };
}

static void Light_EnsureColors(Mesh *mesh)
{
    if (mesh->colors != NULL) return;

    int bytes = mesh->vertexCount * 4;
    mesh->colors = (unsigned char *)MemAlloc((size_t)bytes);
    for (int i = 0; i < bytes; i += 4)
    {
        mesh->colors[i + 0] = 255;
        mesh->colors[i + 1] = 255;
        mesh->colors[i + 2] = 255;
        mesh->colors[i + 3] = 255;
    }
}

void Light_ApplyModel(Model model, const LightEnv *light, Vector3 pointPos, float pointIntensity)
{
    for (int m = 0; m < model.meshCount; m++)
    {
        Mesh *mesh = &model.meshes[m];
        if (mesh->vertices == NULL || mesh->vertexCount <= 0) continue;

        Light_EnsureColors(mesh);

        for (int i = 0; i < mesh->vertexCount; i++)
        {
            Vector3 pos = {
                mesh->vertices[i * 3 + 0],
                mesh->vertices[i * 3 + 1],
                mesh->vertices[i * 3 + 2]
            };
            Vector3 n = { 0.0f, 1.0f, 0.0f };
            if (mesh->normals != NULL)
            {
                n = (Vector3){
                    mesh->normals[i * 3 + 0],
                    mesh->normals[i * 3 + 1],
                    mesh->normals[i * 3 + 2]
                };
            }

            Color c = Light_Tint(WHITE, n, pos, light, pointPos, pointIntensity);
            mesh->colors[i * 4 + 0] = c.r;
            mesh->colors[i * 4 + 1] = c.g;
            mesh->colors[i * 4 + 2] = c.b;
            mesh->colors[i * 4 + 3] = c.a;
        }

        if (mesh->vboId != NULL && mesh->vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR] != 0)
        {
            UpdateMeshBuffer(*mesh, RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR,
                             mesh->colors, mesh->vertexCount * 4, 0);
        }
    }
}

void Light_DrawSun(const LightEnv *light, Vector3 origin, float distance)
{
    if (!light->enabled) return;

    Vector3 sunPos = Vector3Add(origin, Vector3Scale(Vector3Negate(light->direction), distance));
    DrawSphere(sunPos, distance * 0.035f, (Color){ 255, 230, 140, 255 });
    DrawLine3D(origin, sunPos, (Color){ 255, 210, 120, 180 });
}
